/*
 * Copyright (C) 2019 Ola Benderius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <string>

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"
#include "WGS84toCartesian.hpp"

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if (0 == commandlineArguments.count("cid")
      || 0 == commandlineArguments.count("ip")
      || 0 == commandlineArguments.count("port")) {
    std::cerr << argv[0] 
      << " interfaces to the WICE cloud service." 
      << std::endl;
    std::cerr << "Usage:   " << argv[0] << " " 
      << "--cid=<OpenDLV session> "
      << "[--verbose]" 
      << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --verbose" << std::endl;
    retCode = 1;
  } else {
    bool const verbose{commandlineArguments.count("verbose") != 0};
    uint16_t const cid = std::stoi(commandlineArguments["cid"]);

    bool hasPathStart{false};
    std::array<double, 2> pathStart{};
    
    bool wasOut{false};
    uint32_t lapCount{0};

    auto onGeodeticWgs84Reading{[&hasPathStart, &pathStart, &wasOut, &lapCount,
      &verbose](cluon::data::Envelope &&envelope)
      {
        auto msg = cluon::extractMessage<opendlv::proxy::GeodeticWgs84Reading>(
            std::move(envelope));
        if (envelope.senderStamp() == 99 && !hasPathStart) {
          hasPathStart = true;
          pathStart[0] = msg.latitude();
          pathStart[1] = msg.longitude();
        } else {
          std::array<double, 2> pos{msg.latitude(), msg.longitude()};
          auto c = wgs84::toCartesian(pathStart, pos);
          double distance{sqrt(c[0] * c[0] + c[1] * c[1])};
          if (distance > 50.0) {
            wasOut = true;
          }
          if (wasOut && distance < 10.0) {
            wasOut = false;
            lapCount++;
            if (verbose) {
              std::cout << "Drove one more lab, current count is " << lapCount 
                << std::endl;
            }
            if (lapCount % 3 == 0) {
              opendlv::proxy::RemoteMessageRequest rmr;
              rmr.address("0046705294558");
              rmr.message("undock");
            }
          }
        }
      }};

    cluon::OD4Session od4{cid};
    od4.dataTrigger(opendlv::proxy::GeodeticWgs84Reading::ID(),
        onGeodeticWgs84Reading);

    while (od4.isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    retCode = 0;
  }
  return retCode;
}
