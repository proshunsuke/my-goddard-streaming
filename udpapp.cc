/*
 * File:   udpapp.cc
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

#include "udpapp.h"

static class UdpCncClass : public TclClass {
public:
        UdpCncClass() : TclClass("Application/UdpApp") {}
        TclObject* create(int argc, const char*const* argv) {
                if (argc != 5)
                        return NULL;
                Agent *udp = (Agent *)TclObject::lookup(argv[4]);
                if (udp == NULL)
                        return NULL;
                return (new UdpApp(udp));
        }
} class_udpcnc_app;

UdpApp::UdpApp(Agent *udp) : Application()
{
        agent_ = udp;
        agent_->attachApp(this);
}

UdpApp::~UdpApp()
{
        // Before we quit, let our agent know what we no longer exist
        // so that it won't give us a call later...
        agent_->attachApp(NULL);
}

void UdpApp::send(int size, AppData *data)
{
        if (agent_)
	    agent_->sendmsg(size, data, NULL);
	else
	    abort();
}

void UdpApp::process_data(int size, AppData* data)
{
        if (data == NULL)
	    return;
        if (target_)
	    target_->process_data(size, data);
	else {
	    Tcl::instance().eval((const char*)((PacketData*)data)->data());
	}
}

int UdpApp::command(int argc, const char*const* argv)
{
        Tcl& tcl = Tcl::instance();

	if (strcmp(argv[1], "connect") == 0) {
		dst_ = (UdpApp *)TclObject::lookup(argv[2]);
		if (dst_ == NULL) {
			tcl.resultf("%s: connected to null object.", name_);
			return (TCL_ERROR);
		}
		dst_->connect(this);
		return (TCL_OK);
	} else if (strcmp(argv[1], "send") == 0) {
	    int size = atoi(argv[2]);
	    if (argc == 3)
		send(size, NULL);
	    else {
		PacketData* data = new PacketData(1 + strlen(argv[3]));
		strcpy((char*)data->data(), argv[3]);
		send(size, data);
	    }
	    return (TCL_OK);
	} else if (strcmp(argv[1], "dst") == 0) {
		tcl.resultf("%s", dst_->name());
		return TCL_OK;
	}

	return(Application::command(argc, argv));
}
