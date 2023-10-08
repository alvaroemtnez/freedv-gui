//=========================================================================
// Name:            SerialPortRigController.h
// Purpose:         Controls PTT on radios using serial port.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#ifndef SERIAL_PORT_RIG_CONTROLLER_H
#define SERIAL_PORT_RIG_CONTROLLER_H

#include <string>
#include <vector>
#include <mutex>

#include "util/ThreadedObject.h"
#include "IRigController.h"

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#endif

#ifdef _WIN32
#define COM_HANDLE_INVALID			INVALID_HANDLE_VALUE
typedef HANDLE      com_handle_t;
#else
#define COM_HANDLE_INVALID			-1
typedef int         com_handle_t;
#endif

class SerialPortRigController : public ThreadedObject, virtual public IRigController
{
public:
    virtual ~SerialPortRigController();
    
    virtual void connect() override;
    virtual void disconnect() override;

protected:
    SerialPortRigController(std::string serialPort);

    com_handle_t serialPortHandle_;
    
    void raiseDTR_(void);
    void lowerDTR_(void);
    void raiseRTS_(void);
    void lowerRTS_(void);
    
    bool getCTS_(void);

private:
    std::string serialPort_;

    void connectImpl_();
    void disconnectImpl_();
};

#endif // SERIAL_PORT_RIG_CONTROLLER_H
