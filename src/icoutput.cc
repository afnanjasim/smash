/*
 *
 *    Copyright (c) 2014-2018
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/icoutput.h"

#include <fstream>

#include <boost/filesystem.hpp>

#include "smash/action.h"

namespace smash {

ICOutput::ICOutput(const bf::path &path, const std::string &name,
                   const OutputParameters &out_par)
    : OutputInterface(name),
      file_{path / "SMASH_IC.dat", "w"},
      out_par_(out_par) {
  const double prop_time = out_par_.IC_proper_time;
  std::fprintf(file_.get(), "# %s initial conditions\n", VERSION_MAJOR);
  std::fprintf(file_.get(), "# @ proper time: %7.4f fm \n", prop_time);
  std::fprintf(file_.get(), "# tau x y eta mt px py Rap pdg ID charge \n");
  std::fprintf(file_.get(), "# fm fm fm none GeV GeV GeV none none none e \n");
}

ICOutput::~ICOutput() {}

void ICOutput::at_eventstart(const Particles &particles,
                             const int event_number) {
  std::fprintf(file_.get(), "# event %i start\n", event_number + 1);
  SMASH_UNUSED(particles);
};

void ICOutput::at_eventend(const Particles &particles, const int event_number,
                           double impact_parameter, bool empty_event) {
  const auto &log = logger<LogArea::HyperSurfaceCrossing>();
  std::fprintf(file_.get(), "# event %i end\n", event_number + 1);

  bool runtime_too_short = false;
  int below_hypersurface_counter = 0;
  if (particles.size() != 0) {
    for (auto &p : particles) {
      double tau = p.position().tau();
      if (tau < 0.5) {
        runtime_too_short = true;
        below_hypersurface_counter += 1;
      }
    }

    // If the runtime is too short some particles might not yet have
    // reached the hypersurface.
    if (runtime_too_short) {
      log.warn("End time might be too small. ",
               below_hypersurface_counter,
               " particles have not yet crossed the hypersurface.");
    }
  }
  SMASH_UNUSED(particles);
  SMASH_UNUSED(impact_parameter);
  SMASH_UNUSED(empty_event);
};

void ICOutput::at_intermediate_time(const Particles &particles,
                                    const Clock &clock,
                                    const DensityParameters &dens_param) {
  // Dummy, but virtual function needs to be declared.
  // It is never actually used.
  SMASH_UNUSED(particles);
  SMASH_UNUSED(clock);
  SMASH_UNUSED(dens_param);
}

void ICOutput::at_interaction(const Action &action, const double density) {
  SMASH_UNUSED(density);
  assert(action.get_type() == ProcessType::HyperSurfaceCrossing);
  assert(action.incoming_particles().size() == 1);

  ParticleData particle = action.incoming_particles()[0];

  // transverse mass
  double m_trans = sqrt(particle.type().mass() * particle.type().mass() +
                        particle.momentum()[1] * particle.momentum()[1] +
                        particle.momentum()[2] * particle.momentum()[2]);
  // momentum space rapidity
  double rapidity =
      0.5 * log((particle.momentum()[0] + particle.momentum()[3]) /
                (particle.momentum()[0] - particle.momentum()[3]));

  // write particle data
  std::fprintf(file_.get(), "%g %g %g %g %g %g %g %g %s %i %i \n",
               particle.position().tau(), particle.position()[1],
               particle.position()[2], particle.position().eta(), m_trans,
               particle.momentum()[1], particle.momentum()[2], rapidity,
               particle.pdgcode().string().c_str(), particle.id(),
               particle.type().charge());
}

}  // namespace smash
