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

/**
 * \file ConnexionPlugin.cpp
 * \author Michael Rohn
 * \brief A Plugin to use the 3D-Mouse by 3Dconnexion in MARS to control the camera
 */

#include "ConnexionPlugin.h"
#include "ConnexionHID.h"

#include <mars/interfaces/sim/NodeManagerInterface.h>
#include <mars/interfaces/graphics/GraphicsManagerInterface.h>
#include <mars/interfaces/Logging.hpp>
#include <cstdio>

namespace mars {
  namespace plugins {
    namespace connexion_plugin {

      using namespace lib_manager;
      using namespace mars::interfaces;
      using namespace mars::utils;
      using namespace mars::sim;

      ConnexionPlugin::ConnexionPlugin(LibManager *theManager)
        : QThread(),
          MarsPluginTemplateGUI(theManager, std::string("ConnexionPlugin")) {

        open_thread = false;
        thread_closed = false;
        myWidget = NULL;
        object_id = 0;
        win_id = 1;
        object_mode = 1;
        resetCam = false;
        name = "ConnexionPlugin";
        for(int i=0; i<6; i++) {
          use_axis[i] = 1;
          sensitivity[i] = 1.0;
        }
        sensitivity[0] = 2.0;
        sensitivity[1] = 2.0;
        sensitivity[2] = 2.0;

        isInit = false;
        newValues = new connexionValues;
        init();
        useFilter = false;
        filterValue = 0.0;
        syncWithFrames = false;
      }

      void ConnexionPlugin::init() {
        if(!control->graphics || isInit) return;
        // add options to the menu

        camReset();
        if (initConnexionHID(0)) {
          LOG_INFO("%s: Device registered",name.c_str());
          LOG_INFO("%s: running ...",name.c_str());
          isInit = true;
        } else {
          LOG_ERROR("%s: not able to register Device.",name.c_str());
          run_thread = false;
        }

        LOG_INFO("%s: loaded",name.c_str());
        if (isInit) {
          run_thread = true;

          if(gui) {
            std::string tmp = resourcesPath + "/mars/plugins/connexion_plugin/connexion.png";
            gui->addGenericMenuAction("../Plugins/Connexion", 1,
                                      this, 0,
                                      tmp, true);
            if(!control->cfg->getOrCreateProperty("Windows", "ConnexionPlugin/hidden", true).bValue) {
              menuAction(1, false);
            }
          }
          open_thread = true;
        }
        else {
          run_thread = false;
          thread_closed = true;
          LOG_ERROR("%s: init not successful.",name.c_str());
        }

        // set update to gui update
        if (control->graphics) {
          control->graphics->addGraphicsUpdateInterface(this);
        }

      }

      ConnexionPlugin::~ConnexionPlugin(void) {
        //fprintf(stderr, "Delete ConnexionPlugin\n");
        run_thread = false;

        while (!thread_closed && isInit) {
          msleep(10);
        }

        if(isInit) closeConnexionHID();
        delete newValues;
      }

      void ConnexionPlugin::preGraphicsUpdate() {
        double data[7];
        Quaternion q(1.0, 0.0, 0.0, 0.0);
        static Quaternion fq(1.0, 0.0, 0.0, 0.0);
        static Vector fp(0, 0, 0);
        camMutex.lock();
        sReal tmpCamState[7];
        if(open_thread) {
          open_thread = false;
          LOG_INFO("%s: starting ...",name.c_str());
          this->start();
        }
        tmpCamState[0] = camState[0];
        tmpCamState[1] = camState[1];
        tmpCamState[2] = camState[2];
        tmpCamState[3] = camState[3];
        tmpCamState[4] = camState[4];
        tmpCamState[5] = camState[5];
        tmpCamState[6] = camState[6];
        Vector trans(camState[0], camState[1], camState[2]);
        Quaternion qRot(camState[6], camState[3], camState[4], camState[5]);
        camState[0] = 0.0;
        camState[1] = 0.0;
        camState[2] = 0.0;
        camState[3] = 0.0; //q.x
        camState[4] = 0.0; //q.y
        camState[5] = 0.0; //q.z
        camState[6] = 1.0; //q.w
        camMutex.unlock();

        if(useFilter) {
          double b = filterValue;
          if(b < 0) b = 0;
          if(b > 1) b = 1;
          double a = 1-b;
          fq.x() = fq.x()*a + qRot.x()*b;
          fq.y() = fq.y()*a + qRot.y()*b;
          fq.z() = fq.z()*a + qRot.z()*b;
          fq.w() = fq.w()*a + qRot.w()*b;
          fq.normalize();
          fp = fp*a + trans*b;
        }
        else {
          fq = qRot;
          fp = trans;
        }

        if (object_mode == 1) {
          interfaces::GraphicsWindowInterface *gw = control->graphics->get3DWindow(win_id);

          if (gw) {
            if (resetCam) {
              gw->getCameraInterface()->updateViewportQuat(tmpCamState[0],
                                                           tmpCamState[1],
                                                           tmpCamState[2],
                                                           tmpCamState[3],
                                                           tmpCamState[4],
                                                           tmpCamState[5],
                                                           tmpCamState[6]);
              resetCam = false;
            }
            gw->getCameraInterface()->getViewportQuat(data, data+1, data+2,
                                                      data+3, data+4, data+5,
                                                      data+6);
            q = Quaternion(data[6], data[3], data[4], data[5]);
            trans = q*fp;
            q = q * fq;

            data[0] += trans.x();
            data[1] += trans.y();
            data[2] += trans.z();
            gw->getCameraInterface()->updateViewportQuat(data[0], data[1], data[2],
                                                         q.x(), q.y(), q.z(), q.w());
          }
        }
        else if (object_mode == 2) {
          interfaces::GraphicsWindowInterface *gw = control->graphics->get3DWindow(win_id);

          if (gw) {
            gw->getCameraInterface()->getViewportQuat(data, data+1, data+2,
                                                      data+3, data+4, data+5,
                                                      data+6);
            q = Quaternion(data[6], data[3], data[4], data[5]);
            trans = q*trans;
          }
          core_objects_exchange node;
          control->nodes->getNodeExchange(object_id, &node);
          Quaternion tmpQ(node.rot);
          Vector tmpV = node.pos;

          //trans = QVRotate(tmpQ, trans);
          Quaternion qi = q;
          qi.x() *= -1;
          qi.y() *= -1;
          qi.z() *= -1;
          qRot = q * qRot * qi;
          //tmpQ = quad_state;
          tmpQ = qRot * tmpQ;
          //tmpQ = tmpQ*qi;

          tmpV += trans;
          NodeData my_node;
          my_node.index = object_id;
          my_node.pos = tmpV;
          my_node.rot = tmpQ;
          control->nodes->editNode(&my_node, EDIT_NODE_POS | EDIT_NODE_MOVE_ALL);
          control->nodes->editNode(&my_node, EDIT_NODE_ROT | EDIT_NODE_MOVE_ALL);
        }
      }

      void ConnexionPlugin::menuAction(int action, bool checked) {
        switch (action) {
        case 1:
          if (!myWidget) {
            myWidget = new ConnexionWidget(control);
            gui->addDockWidget((void*)myWidget, 0, 5);
            //myWidget->setGeometry(40, 40, 200, 200);
            //connect(myWidget, SIGNAL(hideSignal()), this, SLOT(hideWidget()));
            connect(myWidget, SIGNAL(closeSignal()), this, SLOT(closeWidget()));

            myWidget->setWindowID(win_id);
            myWidget->setObjectID(object_id);
            myWidget->setLockAxis(use_axis);
            myWidget->setSensitivity(sensitivity);

            connect(myWidget, SIGNAL(windowSelected(unsigned long)),
                    this, SLOT(windowSelected(unsigned long)));
            connect(myWidget, SIGNAL(objectSelected(unsigned long)),
                    this, SLOT(objectSelected(unsigned long)));
            connect(myWidget, SIGNAL(setObjectMode(int)),
                    this, SLOT(setObjectMode(int)));
            connect(myWidget, SIGNAL(setLockAxis(int, bool)),
                    this, SLOT(setLockAxis(int, bool)));
            connect(myWidget, SIGNAL(sigSensitivity(int, double)),
                    this, SLOT(setSensitivity(int, double)));
            connect(myWidget, SIGNAL(setUseFilter(bool)),
                    this, SLOT(setUseFilter(bool)));
            connect(myWidget, SIGNAL(setFilterValue(double)),
                    this, SLOT(setFilterValue(double)));
            connect(myWidget, SIGNAL(setSyncWithFrames(bool)),
                    this, SLOT(setSyncWithFrames(bool)));
          }
          else {
            //myWidget->close();
            gui->removeDockWidget((void*)myWidget, 0);
            myWidget = NULL;
            //closeWidget();//myWidget->hide();
          }
          break;
        }
      }

      void ConnexionPlugin::run() {

        while (run_thread) {
          getValue(motion, newValues);

          motion[0] *= sensitivity[0]*use_axis[0] * 0.01;
          motion[1] *= sensitivity[1]*use_axis[1] * 0.01;
          motion[2] *= sensitivity[2]*use_axis[2] * 0.01;
          motion[3] *= sensitivity[3]*use_axis[3] * 0.001;
          motion[4] *= sensitivity[4]*use_axis[4] * 0.001;
          motion[5] *= sensitivity[5]*use_axis[5] * 0.001;

          updateCam(motion);

          if (newValues->button1 == 1) {
            camReset();
          }

          msleep(20);
        }
        LOG_INFO("%s: ... stopped",name.c_str());

        thread_closed = true;
      }


      void ConnexionPlugin::updateCam(sReal motion[6]) {
        sReal tx, ty, tz;
        sReal rx, ry, rz;

        Quaternion q1, q2, q3;
        Vector move, x_axis, y_axis, z_axis;

        tx = motion[0];  //x-axis
        ty = motion[1];  //y-axis
        tz = motion[2];  //z-axis
        rx = motion[3]; //tilt
        ry = motion[4]; //roll
        rz = motion[5]; //spin

        if (fabs(tx) < 0.0001) tx = 0.0;
        if (fabs(ty) < 0.0001) ty = 0.0;
        if (fabs(tz) < 0.0001) tz = 0.0;

        x_axis.x() = 1;
        x_axis.y() = 0;
        x_axis.z() = 0;

        y_axis.x() = 0;
        y_axis.y() = 1;
        y_axis.z() = 0;

        z_axis.x() = 0;
        z_axis.y() = 0;
        z_axis.z() = 1;

        camMutex.lock();
        Quaternion tmpQuatState(camState[6], camState[3],
                                camState[4], camState[5]);

        x_axis = tmpQuatState * x_axis;
        y_axis = tmpQuatState * y_axis;
        z_axis = tmpQuatState * z_axis;

        qFromAxisAndAngle(q1, x_axis, rx);
        qFromAxisAndAngle(q2, y_axis, ry);
        qFromAxisAndAngle(q3, z_axis, -rz);

        if(syncWithFrames) {
          tmpQuatState   = q1 * q2*  q3;
        } else {
          tmpQuatState   = q1 * q2*  q3 * tmpQuatState;
        }
        camState[3] = tmpQuatState.x();
        camState[4] = tmpQuatState.y();
        camState[5] = tmpQuatState.z();
        camState[6] = tmpQuatState.w();

        x_axis *= tx;
        y_axis *= ty;
        z_axis *= -tz;

        move.x() = x_axis.x() + y_axis.x() + z_axis.x(); // left / right
        move.y() = x_axis.y() + y_axis.y() + z_axis.y(); // forward / backward
        move.z() = x_axis.z() + y_axis.z() + z_axis.z(); // up / down

        if(syncWithFrames) {
          camState[0] = move.x(); //x-axis
          camState[1] = move.y(); //y-axis
          camState[2] = move.z(); //z-axis
        }
        else {
          camState[0] += move.x(); //x-axis
          camState[1] += move.y(); //y-axis
          camState[2] += move.z(); //z-axis
        }
        camMutex.unlock();
      }

      void ConnexionPlugin::camReset(void) {
        resetCam = true;
        camMutex.lock();
        camState[0] = 0.0;
        camState[1] = 0.0;
        camState[2] = 2.5;
        camState[3] = 0.0;
        camState[4] = 0.0;
        camState[5] = 0.0;
        camState[6] = 1.0;
        camMutex.unlock();
        resetCam = true;
      }


      void ConnexionPlugin::qFromAxisAndAngle(Quaternion &q, Vector vec,
                                              sReal angle) {
        sReal l = vec.x()*vec.x() + vec.y()*vec.y() + vec.z()*vec.z();
        if (l > 0.0) {
          angle *= 0.5;
          q.w() = cos(angle);
          l = sin(angle) * sqrt(l);
          q.x() = vec.x()*l;
          q.y() = vec.y()*l;
          q.z() = vec.z()*l;
        }
        else {
          q.w() = 1;
          q.x() = 0;
          q.y() = 0;
          q.z() = 0;
        }
      }

      void ConnexionPlugin::hideWidget(void) {
        //fprintf(stderr, "hide event");
        //if (myWidget) myWidget->close();
      }

      void ConnexionPlugin::closeWidget(void) {
        myWidget = NULL;
      }

      void ConnexionPlugin::objectSelected(unsigned long id) {
        object_id = id;
        LOG_DEBUG("id: %lu", id);
      }

      void ConnexionPlugin::windowSelected(unsigned long id) {
        win_id = id;
      }

      void ConnexionPlugin::setObjectMode(int mode) {
        object_mode = mode;
      }

      void ConnexionPlugin::setLockAxis(int axis, bool val) {
        if(axis > 0 && axis < 7) use_axis[axis-1] = val;
      }

      void ConnexionPlugin::setSensitivity(int axis, double sens) {
        if(axis > 0 && axis < 7) sensitivity[axis-1] = sens;
      }

    } // end of namespace connexion_plugin
  } // end of namespace plugins
} // end of namespace mars

DESTROY_LIB(mars::plugins::connexion_plugin::ConnexionPlugin);
CREATE_LIB(mars::plugins::connexion_plugin::ConnexionPlugin);
