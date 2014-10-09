/*
 * File:   mchannel_proc.h
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Multiple communication channel processes implementation
 * This is useful if you want to implement an application
 * that opens and uses more than 1 network connections.
 */

#ifndef ns_mchannel_proc_h
#define ns_mchannel_proc_h

#include "ns-process.h"
#include "app.h"

#define MAX_CHANNEL 4

class MchannelProcess : public Process {
 public:
        MchannelProcess();
	
	// Send media data to the next application in the chain
	virtual void send_data(int channel, int size, AppData* data);
	
	// Process incoming data (both media and control data)
	virtual void process_data(int size, AppData* data);
	
	
 protected:
	virtual int command(int argc, const char*const* argv);
	Process* target_[MAX_CHANNEL]; /* support upto 4 comm channel */
	int ctype_[MAX_CHANNEL];  /* channel type (use packet_t PT_XXX) */
};


// The following TcpAppString class definition is copied from
// webcache/tcpapp.cc.  This is the AppData type TcpApp understands.
// GOOS added an additional method, set_buffer(), to enable general
// application data frame transmissions (not just char string).
class TcpAppString : public AppData {
private:
	int size_;
	char* str_; 
public:
	TcpAppString() : AppData(TCPAPP_STRING), size_(0), str_(NULL) {}
	TcpAppString(TcpAppString& d) : AppData(d) {
		size_ = d.size_;
		if (size_ > 0) {
			str_ = new char[size_];
			strcpy(str_, d.str_);
		} else
			str_ = NULL;
	}
	virtual ~TcpAppString() { 
		if (str_ != NULL) 
			delete []str_; 
	}

	char* str() { return str_; }
	virtual int size() const { return AppData::size() + size_; }

	// Insert string-contents into the ADU
	void set_string(const char* s) {
		if ((s == NULL) || (*s == 0)) 
			str_ = NULL, size_ = 0;
		else {
			size_ = strlen(s) + 1;
			str_ = new char[size_];
			assert(str_ != NULL);
			strcpy(str_, s);
		}
	}
	
	/* GOOS Added method */
	void set_buffer(void* buf, int size) {
		size_ = size;
		str_ = new char[size_];
		memcpy(str_, buf, size_);
	}

	virtual AppData* copy() {
		return new TcpAppString(*this);
	}
};

#endif
