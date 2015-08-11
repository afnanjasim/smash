/*
 *
 *    Copyright (c) 2013-2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */
#include "include/distributions.h"

#include <gsl/gsl_sf_bessel.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "include/constants.h"
#include "include/logging.h"
#include "include/random.h"

namespace Smash {

/* Breit-Wigner distribution */
float breit_wigner(const float srts, const float resonance_mass,
                   const float resonance_width) {
  const float s = srts * srts;
  const float A = s * resonance_width * resonance_width;
  const float B = (s - resonance_mass * resonance_mass);
  return 2. * s * resonance_width / (M_PI * (B*B + A));
}

float cauchy(float x, float pole, float width) {
  const float dm = x - pole;
  return width / (M_PI * (dm*dm + width*width));
}

/* density_integrand - Maxwell-Boltzmann distribution */
double density_integrand(const double energy, const double momentum_sqr,
                         const double temperature) {
  return 4.0 * M_PI * momentum_sqr * exp(-energy / temperature);
}

/* sample_momenta - return thermal momenta */
double sample_momenta(const double temperature, const double mass) {
  const auto &log = logger<LogArea::Distributions>();
  log.debug("Sample momenta with mass ", mass, " and T ", temperature);
  /* Maxwell-Boltzmann average E <E>=3T + m * K_1(m/T) / K_2(m/T) */
  const float m_over_T = mass / temperature;
  const float energy_average = 3 * temperature
                             + mass * gsl_sf_bessel_K1(m_over_T)
                                    / gsl_sf_bessel_Kn(2, m_over_T);
  const float momentum_average_sqr = (energy_average - mass) *
                                     (energy_average + mass);
  const float energy_min = mass;
  const float energy_max = 50.0f * temperature;
  /* double the massless peak value to be above maximum of the distribution */
  const float probability_max = 2.0f * density_integrand(energy_average,
                                                         momentum_average_sqr,
                                                         temperature);

  /* sample by rejection method: (see numerical recipes for more efficient)
   * random momenta and random probability need to be below the distribution */
  float momentum_radial_sqr, probability;
  do {
    float energy = Random::uniform(energy_min, energy_max);
    momentum_radial_sqr = (energy - mass) * (energy + mass);
    probability = density_integrand(energy, momentum_radial_sqr, temperature);
  } while (Random::uniform(0.f, probability_max) > probability);

  return std::sqrt(momentum_radial_sqr);
}

}  // namespace Smash
