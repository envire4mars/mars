/*
 *  Copyright 2011, 2012, DFKI GmbH Robotics Innovation Center
 *
 *  This file is part of the MARS simulation framework.
 *
 *  MARS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3
 *  of the License, or (at your option) any later version.
 *
 *  MARS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with MARS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  NodeGroundContactSensor.h
 *  QTVersion
 *
 *  Created by Malte Roemmermann
 *
 */

#ifndef NODECOMSENSOR_H
#define NODECOMSENSOR_H

#ifdef _PRINT_HEADER_
#warning "NodeCOMSensor.h"
#endif

#include <mars/data_broker/ProducerInterface.h>
#include <mars/data_broker/DataPackageMapping.h>
#include "NodeArraySensor.h"

namespace mars {
  namespace sim {

    class NodeCOMSensor : public NodeArraySensor,
                          public data_broker::ProducerInterface {

    public:
      NodeCOMSensor(interfaces::ControlCenter* control, IDListConfig config);
      ~NodeCOMSensor(void);
      virtual int getAsciiData(char *data) const;
      virtual int getSensorData(interfaces::sReal** data) const;
      static interfaces::BaseSensor* instanciate(interfaces::ControlCenter *control,
                                           interfaces::BaseConfig *config);
      void produceData(const data_broker::DataInfo &info,
                                          data_broker::DataPackage *dbPackage,
                                          int callbackParam);
      void setupDataPackageMapping();

      protected:
        data_broker::DataPackageMapping dbPackageMapping;
        unsigned long int dbPushId;
        utils::Vector com;
        unsigned long linkId;
    };

  } // end of namespace sim
} // end of namespace mars

#endif
