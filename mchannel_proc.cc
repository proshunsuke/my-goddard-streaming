/*
 * File:   mchannel_proc.cc
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Multiple communication channel processes implementation
 * This is useful if you want to implement an application
 * that opens and uses more than 1 network connections.
 */

#include "mchannel_proc.h"
#include "webcache/tcpapp.h"

static class MchannelProcessClass : public TclClass {
public:
       MchannelProcessClass() : TclClass("Process/Mchannel") {}
       TclObject* create(int argc, const char*const* argv) {
	   return (new MchannelProcess());
       }
} class_mchannel_process;


MchannelProcess::MchannelProcess()
{
	for(int i=0; i<MAX_CHANNEL; i++) {
	    target_[i] = NULL;
	    ctype_[i] = -1;
	}
}

void MchannelProcess::send_data(int channel, int size, AppData *data = 0)
{
        if (channel < MAX_CHANNEL && target_[channel])
	    ((Application *)target_[channel])->send(size, data);
	else
	    abort();
}

void MchannelProcess::process_data(int size, AppData* data)
{
        if (data == NULL)
	    return;
        if (data->type() == PACKET_DATA)
	    Tcl::instance().eval((const char*)((PacketData*)data)->data());
	else if (data->type() == TCPAPP_STRING)
	    Tcl::instance().eval(((TcpAppString*)data)->str());
}

int MchannelProcess::command(int argc, const char*const* argv)
{
        Tcl& tcl = Tcl::instance();

	if(strcmp(argv[1], "channel_type") == 0) {
	    if (argc == 4) {
		int i = atoi(argv[2]);
		if (i < 0 || i >= MAX_CHANNEL) {
		    fprintf(stderr, "Invalid channel %s\n", argv[2]);
		    abort();
		}
		ctype_[i] = atoi(argv[3]);
		//printf("MchannalProcess: Ch[%d] = %d\n", i, ctype_[i]);
		return TCL_OK;
	    }
	}
	else if (strcmp(argv[1], "channel") == 0) {
	    if (argc == 3 || argc == 4) {
		int i = atoi(argv[2]);
		if (i < 0 || i >= MAX_CHANNEL) {
		    fprintf(stderr, "Invalid channel %s\n", argv[2]);
		    abort();
		}
                if (argc == 3) {
		    Process *p = target_[i];
		    tcl.resultf("%s", p ? p->name() : "");
		    return TCL_OK;
                } else if (argc == 4) {
		    Process *p = (Process *)TclObject::lookup(argv[3]);
		    if (p == NULL) {
			fprintf(stderr, "Non-existent process %s\n", argv[3]);
			abort();
		    }
		    target_[i] = p;
		    ctype_[i] = (int)((Application*)p)->get_agent()->get_pkttype();
		    /* since two-way comm is expected, PT_ACK is not used */
		    if (ctype_[i] == (int)PT_ACK) {
			ctype_[i] = (int)PT_TCP;
			((Application*)p)->get_agent()->set_pkttype(PT_TCP);
		    } 
		    //printf("MchannalProcess: Ch[%d] = %d\n", i, ctype_[i]);
		    tcl.evalf("%s target %s", target_[i]->name(), this->name());
		    return TCL_OK;
                }
	    }
        }
	else if (strcmp(argv[1], "send_msg") == 0) {
	    if (argc == 5) {
		int channel = atoi(argv[2]);
		int size = atoi(argv[3]);
		if(ctype_[channel] == PT_TCP) { /* TCP channel */
		    TcpAppString* data = new TcpAppString();
		    data->set_string(argv[4]);
		    send_data(channel, size, data);
		}
		else { /* Other channels */
		    PacketData* data = new PacketData(1 + strlen(argv[4]));
		    strcpy((char*)data->data(), argv[4]);
		    send_data(channel, size, data);
		}
		return (TCL_OK);
	    }
	}

	return(Process::command(argc, argv));
}
