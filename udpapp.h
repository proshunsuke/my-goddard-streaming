/*
 * File:   udpapp.h
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Through this "Application/UdpApp", applications can send
 * "application data" in simulations.  Indeed, applications
 * can directlry use "Agent/UDP" to send data.  However,
 * "Application/UdpApp" gives the same interface as
 * "Application/TcpApp".  This uniform interface is useful for
 * the implementation of multi-channel process that uses more
 * than 1 network connections (different types).
 */

#ifndef ns_udpapp_h
#define ns_udpapp_h

#include "app.h"

class UdpApp : public Application {
 public:
        UdpApp(Agent *udp);
	~UdpApp();
	
        void connect(UdpApp *dst) { dst_ = dst; }

	// Process incoming data
	virtual void process_data(int size, AppData* data);
	
	// Send data to underlying agent
        virtual void send(int size, AppData *data);

 protected:
	virtual int command(int argc, const char*const* argv);

        // We don't have start/stop
	virtual void start() { abort(); }
	virtual void stop() { abort(); }

	UdpApp *dst_;
};

#endif
