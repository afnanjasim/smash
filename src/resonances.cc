/*
 *
 *    Copyright (c) 2013
 *      maximilian attems <attems@fias.uni-frankfurt.de>
 *      Jussi Auvinen <auvinen@fias.uni-frankfurt.de>
 *
 *    GNU General Public License (GPLv3)
 *
 */
#include "include/resonances.h"

#include <gsl/gsl_sf_coupling.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "include/constants.h"
#include "include/distributions.h"
#include "include/FourVector.h"
#include "include/macros.h"
#include "include/outputroutines.h"
#include "include/ParticleData.h"
#include "include/ParticleType.h"

/* resonance_cross_section - energy-dependent cross section
 * for producing a resonance
 */
std::map<int, double> resonance_cross_section(
  const ParticleData &particle1, const ParticleData &particle2,
  const ParticleType &type_particle1, const ParticleType &type_particle2,
  const Particles &particles) {
  const int charge1 = type_particle1.charge(),
    charge2 = type_particle2.charge();

  /* Isospin z-component based on Gell-Mann–Nishijima formula
   * 2 * Iz = 2 * charge - (baryon number + strangeness + charm)
   * XXX: Strangeness and charm ignored for now!
   */
  const int isospin_z1 = type_particle1.spin() % 2 == 0
                         ? charge1 * 2
                         : charge1 * 2 - type_particle1.pdgcode()
                                       / abs(type_particle1.pdgcode());
  const int isospin_z2 = type_particle2.spin() % 2 == 0
                         ? charge2 * 2
                         : charge2 * 2 - type_particle2.pdgcode()
                                       / abs(type_particle2.pdgcode());
  std::map<int, double> possible_resonances;

  /* key 0 refers to total resonance production cross section */
  possible_resonances[0] = 0.0;

  /* Resonances do not form resonances */
  if (type_particle1.width() > 0.0 || type_particle2.width() > 0.0)
    return possible_resonances;

  /* No baryon-baryon interactions for now */
  if (type_particle1.spin() % 2 != 0 && type_particle2.spin() % 2 != 0)
    return possible_resonances;

  /* Isospin symmetry factor */
  int symmetryfactor = 1;
  if (type_particle1.isospin() == type_particle2.isospin())
    symmetryfactor = 2;

  /* Mandelstam s = (p_a + p_b)^2 = square of CMS energy */
  const double mandelstam_s =
       (particle1.momentum() + particle2.momentum()).Dot(
         particle1.momentum() + particle2.momentum());

  /* CM momentum */
  const double cm_momentum_squared
    = (particle1.momentum().Dot(particle2.momentum())
       * particle1.momentum().Dot(particle2.momentum())
       - type_particle1.mass() * type_particle1.mass()
       * type_particle2.mass() * type_particle2.mass()) / mandelstam_s;

  /* Find all the possible resonances */
  for (std::map<int, ParticleType>::const_iterator
       i = particles.types_cbegin(); i != particles.types_cend(); ++i) {
       ParticleType type_resonance = i->second;
    /* Not a resonance, go to next type of particle */
    if (type_resonance.width() < 0.0)
      continue;

    /* Check for charge conservation */
    if (type_resonance.charge() != charge1 + charge2)
      continue;

    /* Check for baryon number conservation */
    if (type_particle1.spin() % 2 != 0 || type_particle2.spin() % 2 != 0) {
      /* Step 1: We must have fermion */
      if (type_resonance.spin() % 2 == 0) {
        continue;
      }
      /* Step 2: We must have antiparticle for antibaryon
       * (and non-antiparticle for baryon)
       */
      if (type_particle1.spin() % 2 != 0
          && (std::signbit(type_particle1.pdgcode())
          != std::signbit(type_resonance.pdgcode()))) {
        continue;
      } else if (type_particle2.spin() % 2 != 0
          && (std::signbit(type_particle2.pdgcode())
          != std::signbit(type_resonance.pdgcode()))) {
        continue;
      }
    }

    int isospin_z_resonance = (type_resonance.spin()) % 2 == 0
     ? type_resonance.charge() * 2
     : type_resonance.charge() * 2 - type_resonance.pdgcode()
                                    / abs(type_resonance.pdgcode());

    /* Calculate isospin Clebsch-Gordan coefficient
     * (-1)^(j1 - j2 + m3) * sqrt(2 * j3 + 1) * [Wigner 3J symbol]
     * Note that the calculation assumes that isospin values
     * have been multiplied by two
     */
    double wigner_3j =  gsl_sf_coupling_3j(type_particle1.isospin(),
       type_particle2.isospin(), type_resonance.isospin(),
       isospin_z1, isospin_z2, -isospin_z_resonance);
    double clebsch_gordan_isospin = 0.0;
    if (fabs(wigner_3j) > really_small)
      clebsch_gordan_isospin = pow(-1, type_particle1.isospin() / 2.0
      - type_particle2.isospin() / 2.0 + isospin_z_resonance / 2.0)
      * sqrt(type_resonance.isospin() + 1) * wigner_3j;

    printd("CG: %g I1: %i I2: %i IR: %i iz1: %i iz2: %i izR: %i \n",
         clebsch_gordan_isospin,
         type_particle1.isospin(), type_particle2.isospin(),
         type_resonance.isospin(),
         isospin_z1, isospin_z2, isospin_z_resonance);

    /* If Clebsch-Gordan coefficient is zero, don't bother with the rest */
    if (fabs(clebsch_gordan_isospin) < really_small)
      continue;

    /* Calculate spin factor */
    const double spinfactor = (type_resonance.spin() + 1)
      / ((type_particle1.spin() + 1) * (type_particle2.spin() + 1));

    float resonance_width = type_resonance.width();
    float resonance_mass = type_resonance.mass();
    /* Calculate resonance production cross section
     * using the Breit-Wigner distribution as probability amplitude
     */
    double resonance_xsection =  clebsch_gordan_isospin * clebsch_gordan_isospin
         * spinfactor * symmetryfactor
         * 4.0 * M_PI / cm_momentum_squared
         * breit_wigner(mandelstam_s, resonance_mass, resonance_width)
         * hbarc * hbarc / fm2_mb;

    /* If cross section is non-negligible, add resonance to the list */
    if (resonance_xsection > really_small) {
      possible_resonances[type_resonance.pdgcode()] = resonance_xsection;
      possible_resonances[0] += resonance_xsection;
      printd("Found resonance %i (%s) with mass %f and width %f.\n",
             type_resonance.pdgcode(), type_resonance.name().c_str(),
             resonance_mass, resonance_width);
      printd("Original particles: %s %s Charges: %i %i \n",
             type_particle1.name().c_str(), type_particle2.name().c_str(),
             type_particle1.charge(), type_particle2.charge());
    }
  }
  return possible_resonances;
}

/* 1->2 resonance decay process */
int resonance_decay(Particles *particles, int particle_id) {
  /* Add two new particles */
  int new_id_a = particles->add_data();
  int new_id_b = particles->add_data();

  const int charge = particles->type(particle_id).charge();
  const double total_energy = particles->data(particle_id).momentum().x0();
  int type_a = 0, type_b = 0;
  /* XXX: Can the hardcoding of decay channels be avoided? */
  if (particles->type(particle_id).spin() % 2 == 0) {
    /* meson resonance decays into pions */
    if (charge == 0) {
      type_a = 211;
      type_b = -211;
    } else if (charge == 1) {
      type_a = 211;
      type_b = 111;
    } else if (charge == -1) {
      type_a = -211;
      type_b = 111;
    }
  } else if (particles->data(particle_id).pdgcode() > 0) {
    /* Baryon resonance decays into pion and baryon */
    if (charge == 0) {
      type_a = 2212;
      type_b = -211;
      /* If there's not enough energy, use the lighter combination */
      if (unlikely(particles->type(type_a).mass()
                   + particles->type(type_b).mass()
                   > total_energy)) {
        type_a = 2112;
        type_b = 111;
      }
    } else if (charge == 1) {
      type_a = 2112;
      type_b = 211;
      if (unlikely(particles->type(type_a).mass()
                   + particles->type(type_b).mass()
                   > total_energy)) {
        type_a = 2212;
        type_b = 111;
      }
    } else if (charge == -1) {
      type_a = 2112;
      type_b = -211;
    } else if (charge == 2) {
      type_a = 2212;
      type_b = 211;
    }
  } else {
    /* Antibaryon resonance decays into pion and antibaryon */
    if (charge == 0) {
      type_a = -2212;
      type_b = 211;
      if (unlikely(particles->type(type_a).mass()
                   + particles->type(type_b).mass()
                   > total_energy)) {
        type_a = -2112;
        type_b = 111;
      }
    } else if (charge == 1) {
      type_a = -2112;
      type_b = 211;
    } else if (charge == -1) {
      type_a = -2112;
      type_b = -211;
      if (unlikely(particles->type(type_a).mass()
                   + particles->type(type_b).mass()
                   > total_energy)) {
        type_a = -2212;
        type_b = 111;
      }
    } else if (charge == -2) {
      type_a = -2212;
      type_b = -211;
    }
  }
  particles->data_pointer(new_id_a)->set_pdgcode(type_a);
  particles->data_pointer(new_id_b)->set_pdgcode(type_b);

  double mass_a = particles->type(new_id_a).mass(),
    mass_b = particles->type(new_id_b).mass();
  double energy_a = (total_energy * total_energy
                     + mass_a * mass_a - mass_b * mass_b)
                    / (2.0 * total_energy);

  double momentum_radial = sqrt(energy_a * energy_a - mass_a * mass_a);
  /* phi in the range from [0, 2 * pi) */
  double phi = 2.0 * M_PI * drand48();
  /* cos(theta) in the range from [-1.0, 1.0) */
  double cos_theta = -1.0 + 2.0 * drand48();
  double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
  if (energy_a  < mass_a || abs(cos_theta) > 1) {
    printf("Particle %d radial momenta %g phi %g cos_theta %g\n", new_id_a,
         momentum_radial, phi, cos_theta);
    printf("Etot: %g m_a: %g m_b %g E_a: %g", total_energy, mass_a, mass_b,
           energy_a);
  }
  particles->data_pointer(new_id_a)->set_momentum(mass_a,
      momentum_radial * cos(phi) * sin_theta,
      momentum_radial * sin(phi) * sin_theta,
      momentum_radial * cos_theta);
  particles->data_pointer(new_id_b)->set_momentum(mass_b,
    - particles->data(new_id_a).momentum().x1(),
    - particles->data(new_id_a).momentum().x2(),
    - particles->data(new_id_a).momentum().x3());

  /* Both decay products begin from the same point */
  FourVector decay_point = particles->data(particle_id).position();
  particles->data_pointer(new_id_a)->set_position(decay_point);
  particles->data_pointer(new_id_b)->set_position(decay_point);

  /* No collision yet */
  particles->data_pointer(new_id_a)->set_collision(-1, 0, -1);
  particles->data_pointer(new_id_b)->set_collision(-1, 0, -1);

  printd("Created %s and %s with IDs %d and %d \n",
    particles->type(new_id_a).name().c_str(),
    particles->type(new_id_b).name().c_str(), new_id_a, new_id_b);

  return new_id_a;
}

/* 2->1 resonance formation process */
int resonance_formation(Particles *particles, int particle_id, int other_id,
  int pdg_resonance) {
  /* Add a new particle */
  int new_id = particles->add_data();
  particles->data_pointer(new_id)->set_pdgcode(pdg_resonance);

  /* Center-of-momentum frame of initial particles
   * is the rest frame of the resonance
   */
  const double energy = particles->data(particle_id).momentum().x0()
    + particles->data(other_id).momentum().x0();
  /* We use fourvector to set 4-momentum, as setting it
   * with doubles requires that particle is on
   * mass shell, which is not generally true for resonances
   */
  FourVector resonance_momentum(energy, 0.0, 0.0, 0.0);
  particles->data_pointer(new_id)->set_momentum(resonance_momentum);

  printd("Momentum of the new particle: %g %g %g %g \n",
    particles->data(new_id).momentum().x0(),
    particles->data(new_id).momentum().x1(),
    particles->data(new_id).momentum().x2(),
    particles->data(new_id).momentum().x3());

  /* The real position should be between parents in the computational frame! */
  particles->data_pointer(new_id)->set_position(1.0, 0.0, 0.0, 0.0);

  /* No collision yet */
  particles->data_pointer(new_id)->set_collision(-1, 0, -1);

  printd("Created %s with ID %i \n", particles->type(new_id).name().c_str(),
         particles->data(new_id).id());

  return new_id;
}