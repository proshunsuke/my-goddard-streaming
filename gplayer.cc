/*
 * File:   gplayer.cc
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Goddard client (Gplayer) implementation
 */

#include "gplayer.h"
#include "random.h"
#include <values.h>

static class GplayerClass : public TclClass {
public:
    GplayerClass() : TclClass("Process/Mchannel/Gplayer") {}
    TclObject* create(int argc, const char*const* argv) {
	return (new Gplayer());
    }
} class_gplayer_process;


// When upscale_timer_ expires call Gplayer::process_upscale()
void UpscaleTimer::expire(Event*)
{
    t_->process_upscale();
}

// When pktpair_timer_ expires call Gplayer::pktpair_timeout_handler()
void PktpairTimer::expire(Event*)
{
    t_->pktpair_timeout_handler();
}

// When pktpair_timer_ expires call Gplayer::pktpair_timeout_handler()
void DisplayTimer::expire(Event*)
{
    t_->play_video();
}


Gplayer::Gplayer() : MchannelProcess(), upscale_timer_(this), 
		     pktpair_timer_(this), display_timer_(this), tchan_(0)
{
    bind("play_buf_len_in_sec_", &gpp_.play_buf_len);
    bind("buffer_factor_", &gpp_.buf_factor);
    bind("pkp_timeout_interval_", &gpp_.pktpair_timeout);
    bind("upscale_interval_", &gpp_.upscale_interval);
    bind("loss_monitor_interval_", &gpp_.loss_mon_interval);
    bind("upscale_frame_loss_rate_", &gpp_.loss_rate_min_thrsh);
    bind("upscale_limit_time_factor_", &gpp_.upscale_lt_factor);
    bind("downscale_frame_loss_rate_", &gpp_.loss_rate_max_thrsh);

    gpv_.state = STOPPED;
}

void Gplayer::init()
{
    gpv_.state = STOPPED;
    gpv_.eos == FALSE;
    gpv_.scale = 0;
    gpv_.exp_mseq = 0;
    gpv_.exp_cseq = 0;
    gpv_.exp_pseq = 0;
    gpv_.last_cseq = -1;
    gpv_.t_pktpair_1st = 0;
    gpv_.congestion = FALSE;

    gps_.est_cap_max = 0;
    gps_.est_cap_min = MAXDOUBLE;

    gps_.t_play = MAXDOUBLE;
    gps_.rebuf_cnt = 0;
    gps_.rebuf_scale = MAXINT;

    gps_.t_last = 0.0;
    gps_.t_last_frm = 0.0;
    gps_.est_cap = 0.0;
    gps_.rcvd_bytes = 0;
    gps_.rcvd_frms = 0;
    gps_.lost_frms = 0;

    fasm_.seq = -1;
    fasm_.exp_nsegs = -1;
    fasm_.rcv_nsegs = -1;
    fasm_.rcv_bytes = -1;
}

//////////////////////////////////////////////////////////
// Periodically update stats and makes scale UP decision
void Gplayer::process_upscale()
{
    if (gpv_.state != STOPPED) {
	double now = Scheduler::instance().clock();
        //give another chance to stream with a low rebuf scale
	if ((gpv_.state == STREAMING) &&
	    (now - gps_.t_play >= gpp_.upscale_lt_factor * gpp_.upscale_interval) &&
	    (gps_.rebuf_scale < gpv_.maxscale) &&
	    (gpv_.scale+1 < gps_.rebuf_scale)) {
	    if (gpv_.congestion == FALSE)
		gps_.rebuf_scale++;
	    gps_.t_play = now;
	    gpv_.congestion == FALSE;
	}
	//future work: which stats to use for scale up decision?
	//currently frame loss, rebuf count and estimated capacity is used.
	double p = (double)gps_.lost_frms /
	           (double)(gps_.lost_frms + gps_.rcvd_frms);
	if ((gpv_.state == STREAMING) &&  //no scale up on buffering
	    (p < gpp_.loss_rate_min_thrsh) &&   //check frame loss
	    (gpv_.scale+1 < gps_.rebuf_scale) &&  //check last rebuf scale
	    (gpv_.bitrate[gpv_.scale+1] < gps_.est_cap_max) && //chk est cap
	    (gpv_.scale+1 < gpv_.maxscale)) {
	    gpv_.scale++;
	    char buf[MAX_MSGLEN];
	    sprintf(buf, "SSET %d\0", gpv_.scale);
	    send_ctrmsg(buf);
	}
	//reset gplayer statistics
	gps_.t_last = now;
	gps_.t_last_frm = now; 
	gps_.rcvd_bytes = 0;
	gps_.rcvd_frms = 0;
	gps_.lost_frms = 0;   
	
	//reschedule the send_pkt timer
	upscale_timer_.resched(gpp_.upscale_interval);
    }
}

void Gplayer::pktpair_timeout_handler()
{
    send_ctrmsg("PKTP");
    pktpair_timer_.resched(gpp_.pktpair_timeout);
}

void Gplayer::process_data(int size, AppData* data)
{
    struct goddard_frame *frm;

    if (data == NULL)
	return;
    if (data->type() == TCPAPP_STRING)
	frm = (struct goddard_frame*)((TcpAppString*)data)->str();
    else if (data->type() == PACKET_DATA)
	frm = (struct goddard_frame*)((PacketData*)data)->data();
    else {
	fprintf(stderr, "Gplayer: unknown data type received\n");
	abort();
    }

    // Process message from goddard server
    if (frm->type == CNTRL_DATA) process_ctrmsg(frm);
    else if (frm->type == MEDIA_DATA) process_medframe(frm);
    else if (frm->type == PPAIR_DATA) process_pktpair(frm);
    else {
	fprintf(stderr, "Gplayer: unknown frame type received\n");
	abort();
    }
}

void Gplayer::process_ctrmsg(struct goddard_frame *frm)
{
    sprintf(tmsg_,"%f Gplayer %s recv ctr-frame seq: %d size: %d msg: %s\n",
	   Scheduler::instance().clock(), this->name(), 
	   frm->seq, frm->size, frm->u.ctr.msg);
    gtrace();

    int argc; char *argv[5];
    argv[0] = strtok(frm->u.ctr.msg, " ");
    for (argc=1; argc<5; argc++) {
	argv[argc] = strtok(NULL, " ");
	if (argv[argc] == NULL) break;
    }
    
    if (strncmp(argv[0], "READY", 5) == 0) {
	//got ready msg from server, request packet pair
	send_ctrmsg("PKTP");
	pktpair_timer_.resched(gpp_.pktpair_timeout);
    }
    else if (strncmp(argv[0], "STOP", 4) == 0) {
	//change player state
	gpv_.state = STOPPED;
	gpv_.eos == TRUE;
    }
    else if (strncmp(argv[0], "MAXL", 4) == 0 && argc == 2) {
	//init the gplayer and set the number of scale levels
	init();
	gpv_.maxscale = atoi(argv[1]);
	gpv_.scale = gpv_.maxscale - 1; //set current scale to maxscale
	if (gpv_.maxscale < 0 || gpv_.maxscale > MAX_SCALE) {
	    fprintf(stderr, "Gplayer: scale value is out of range.\n");
	    abort();
	}
    }
    else if (strncmp(argv[0], "RATE", 4) == 0 && argc == 4) {
	//set the number of scale levels
	int i = atoi(argv[1]);
	if (i < 0 || i >= gpv_.maxscale) {
	    fprintf(stderr, "Gplayer: index is out of range.\n");
	    abort();
	}
	gpv_.bitrate[i] = atof(argv[2]);
	gpv_.fps[i] = atoi(argv[3]);
    }
    else {
	fprintf(stderr, "Gplayer: unknown control message: \n", argv[0]);
	abort();
    }
}

void Gplayer::process_medframe(struct goddard_frame *frm)
{
   //printf("%f Gplayer %s recv med-frame seq: %d seg: %d size: %d inter: %f\n",
   //     Scheduler::instance().clock(), this->name(),
   //frm->seq, frm->u.med.seg_cur, frm->size, frm->u.med.interval);

    double now = Scheduler::instance().clock();

    //update bytes received
    gps_.rcvd_bytes += frm->size;

    //perform segment assembly (need for non-TCP channel)
    if (fasm_.seq == frm->seq) {
	fasm_.rcv_nsegs++;
	fasm_.rcv_bytes += frm->size;
    }
    else {
	fasm_.seq = frm->seq;
	fasm_.exp_nsegs = frm->u.med.seg_last + 1;
	fasm_.rcv_nsegs = 1;
	fasm_.rcv_bytes = frm->size;
    }

    //do we have a full frame?
    if (fasm_.rcv_nsegs == fasm_.exp_nsegs) {
	sprintf(tmsg_,"%f Gplayer %s recv med-frame seq: %d size: %d inter: %f\n",
	       Scheduler::instance().clock(), this->name(),
	       frm->seq, fasm_.rcv_bytes, frm->u.med.interval);
	gtrace();

	//update frame statistics
	gps_.t_last_frm = now;
	gps_.rcvd_frms++;
	gps_.lost_frms += frm->seq - gpv_.exp_mseq;
	
	//if frame loss detected, and some time has passed adjust the
	//scale level.  If scale level is adjusted, notify the server.
	if(gps_.lost_frms && (now > gps_.t_last + gpp_.loss_mon_interval)) {
	    //compute frame loss rate
	    double p = (double)gps_.lost_frms /
		       (double)(gps_.lost_frms + gps_.rcvd_frms);
	    //compute frame loss threshold
	    double p_thresh = gpp_.loss_rate_max_thrsh;
	    if (gpv_.scale > 0) {
		p_thresh = gpv_.fps[gpv_.scale]*(1 - gpp_.loss_rate_max_thrsh);
		p_thresh -= gpv_.fps[gpv_.scale - 1];
		p_thresh /= (double)gpv_.fps[gpv_.scale];
		if (p_thresh < gpp_.loss_rate_max_thrsh)
		    p_thresh = gpp_.loss_rate_max_thrsh;
	    }
            //if frame loss rate intollerable
	    if(p > p_thresh) {
		if(adjust_scale_level(1)) {
		    //notify server scale level to stream
		    char buf[MAX_MSGLEN];
		    sprintf(buf, "SSET %d\0", gpv_.scale);
		    send_ctrmsg(buf);
		}
		//notify the up-scale unit
		gpv_.congestion == TRUE;

		//reset gplayer statistics
		gps_.t_last = now;
		gps_.t_last_frm = now;
		gps_.rcvd_bytes = 0;
		gps_.rcvd_frms = 0;
		gps_.lost_frms = 0;

		//restart upscale timer
		upscale_timer_.resched(gpp_.upscale_interval);
	    }
	}

	//update the next expected media frame number
	gpv_.exp_mseq = frm->seq + 1;

	//enqueue frame into the playout buffer
	struct gplayer_frame *gfrm = new (struct gplayer_frame);
	gfrm->seq      = frm->seq;
	gfrm->size     = fasm_.rcv_bytes;
	gfrm->interval = frm->u.med.interval;
	gfrm->next     = NULL;
	play_q_.enque(gfrm);

        //if BUFFERING or STOPPED (end of streaming) and the play queue
        //is filled, start playing video.  If gplayer state was BUFFERING,
	//notify the server to change the state to STREAMING.
	if ((gpv_.state != STREAMING) && 
	    (play_q_.qlen_in_time() > gpp_.play_buf_len)) {
	    if (gpv_.state == BUFFERING) {
		gpv_.state = STREAMING;
		gps_.t_play = now;
		send_ctrmsg("STRM");
		play_video();
	    }
	    else if (display_timer_.status() == TIMER_IDLE) {
                //gpv_.state == STOPPED and vido play is not scheduled 
		sprintf(tmsg_,"%f Gplayer %s playing the remaining STRM\n",
		       Scheduler::instance().clock(), this->name());
		gtrace();
		play_video();
	    }
	}
    }
    else { //we have a partial frame
	//if no full frame for (2*loss_mon_interval), then timeout
	if (now > gps_.t_last_frm + 2*gpp_.loss_mon_interval) {
	    if(adjust_scale_level(0)) {
		//notify server scale level to stream
		char buf[MAX_MSGLEN];
		sprintf(buf, "SSET %d\0", gpv_.scale);
		send_ctrmsg(buf);
	    }
	    //notify the up-scale unit
	    gpv_.congestion == TRUE;
	    
	    //reset gplayer statistics
	    gps_.t_last = now;
	    gps_.t_last_frm = now;
	    gps_.rcvd_bytes = 0;
	    gps_.rcvd_frms = 0;
	    gps_.lost_frms = 0;
	    
	    //restart upscale timer
	    upscale_timer_.resched(gpp_.upscale_interval);
	}
    }
}

void Gplayer::play_video()
{
    double tlen_before, tlen_after, next_time;

    //printf("%f Gplayer %s play_qlen: %d %f\n", Scheduler::instance().clock(),
    //       this->name(), play_q_.qlen(), play_q_.qlen_in_time());

    if ((tlen_before = play_q_.qlen_in_time()) == 0.0) {
	if (gpv_.state == STOPPED) {
	    sprintf(tmsg_,"%f Gplayer %s halt playing STRM - rebufcnt: %d\n",
		   Scheduler::instance().clock(),this->name(),gps_.rebuf_cnt);
	    gtrace();
	}
	else {
	    send_ctrmsg("PKTP");
	    pktpair_timer_.resched(gpp_.pktpair_timeout);
	}
    }
    else {
	struct gplayer_frame *gfrm = play_q_.deque();
	sprintf(tmsg_,"%f Gplayer %s play med-frame seq: %d size: %d inter: %f\n",
	       Scheduler::instance().clock(), this->name(),
	       gfrm->seq, gfrm->size, gfrm->interval);
	gtrace();
	delete gfrm;
	tlen_after = play_q_.qlen_in_time();
	next_time = tlen_before - tlen_after;
	if (next_time > 0)
	    display_timer_.resched(next_time);
	else {
	    fprintf(stderr,"Gplayer: error in display_timer scheduling\n");
	    abort();
	}
    }
}

int Gplayer::adjust_scale_level(int gentle)
{
    double now = Scheduler::instance().clock();
    if (now - gps_.t_last <= 0) return FALSE;

    //find a scale level of which the streaming bitrate is less than
    //the minimum of the measured throughput (if data available) and
    //estimated bandwidth.

    double target_r = (double)gps_.rcvd_bytes / (now - gps_.t_last);
    if (target_r == 0 || gps_.est_cap < target_r) target_r = gps_.est_cap;
    int scale;
    for (scale=gpv_.maxscale -1; scale>0; scale--) {
	if (gpv_.bitrate[scale] <= target_r) break;
        //if ((gpv_.bitrate[scale]*gpp_.buf_factor) < target_r) break;
    }
    if(scale < gpv_.scale) {
	if (gentle)
	    gpv_.scale--;
	else
	    gpv_.scale = scale;
	return TRUE;
    }

    return FALSE;
}

void Gplayer::process_pktpair(struct goddard_frame *frm)
{
    double now = Scheduler::instance().clock();

    sprintf(tmsg_,"%f Gplayer %s recv pkp-frame seq: %d size: %d\n", now,
	   this->name(), frm->seq, frm->size);
    gtrace();

    //got first packet of the pair
    if ((frm->seq % 2) == 0) {
	gpv_.t_pktpair_1st = now;
	gpv_.exp_pseq = frm->seq + 1;
    }
    else { //got second packet of a pair
	if (gpv_.exp_pseq != frm->seq) return;

	//cancel pktpair wait timeout timer (since we got a pair)
	pktpair_timer_.cancel();

	//estimate bottleneck capacity
	gps_.est_cap = (double)frm->size / (now - gpv_.t_pktpair_1st);
	if(gps_.est_cap > gps_.est_cap_max) gps_.est_cap_max = gps_.est_cap;
	if(gps_.est_cap < gps_.est_cap_min) gps_.est_cap_min = gps_.est_cap;
	sprintf(tmsg_,"%f Gplayer %s estimated capacity: %f MBps\n", now,
	       this->name(), gps_.est_cap);
	gtrace();

	//choose scale level
	int scale_old = gpv_.scale;
	adjust_scale_level(0);
	
        //reset gplayer statistics
	gps_.t_last = now;
	gps_.t_last_frm = now;
	gps_.rcvd_bytes = 0;
	gps_.rcvd_frms = 0;
	gps_.lost_frms = 0;
	
	//notify server scale level to stream
	char buf[MAX_MSGLEN];
	sprintf(buf, "SSET %d\0", gpv_.scale);
	send_ctrmsg(buf);

	//notify server buffer factor
	sprintf(buf, "BFCT %f\0", gpp_.buf_factor);
	send_ctrmsg(buf);

        //ask server to start or rebuffer in case of streaming
	if (gpv_.state == STOPPED && gpv_.eos == FALSE)
	    send_ctrmsg("PLAY");
	else {//state == STREAMING
	    gps_.rebuf_cnt++;
	    gps_.rebuf_scale = scale_old;
	    send_ctrmsg("BUFF");
	}
        //set gplayer state to buffering
	gpv_.state = BUFFERING;

	//start (restart) upscale timer
	upscale_timer_.resched(gpp_.upscale_interval);
    }
}

void Gplayer::send_ctrmsg(const char *msg)
{
    if (strlen(msg) > MAX_MSGLEN) {
        fprintf(stderr, "Gplayer: strlen(ctr_msg) > %d\n", MAX_MSGLEN);
        abort();
    }

    //construct control message
    struct goddard_frame frm;
    frm.type = CNTRL_DATA;
    frm.size = sizeof(frm);
    frm.seq = ++gpv_.last_cseq;
    strcpy(frm.u.ctr.msg, msg);

    //trace control message
    sprintf(tmsg_,"%f Gplayer %s send ctr-frame seq: %d size: %d msg: %s\n",
	   Scheduler::instance().clock(), this->name(), 
	   frm.seq, frm.size, frm.u.ctr.msg);
    gtrace();

    //send control message
    TcpAppString* data = new TcpAppString();
    data->set_buffer((void*)&frm, sizeof(frm));
    send_data(CNTRL_CH, frm.size, data);
}


int Gplayer::command(int argc, const char*const* argv)
{
        Tcl& tcl = Tcl::instance();

	
	if (argc == 3) {
	    // attach a file for variable tracing
	    if (strcmp(argv[1], "attach") == 0) {
		int mode;
		const char* id = argv[2];
		tchan_ = Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
		if (tchan_ == 0) {
		    tcl.resultf("RED: trace: can't attach %s for writing", id);
		    return (TCL_ERROR);
		}
		return (TCL_OK);
	    }
	}

	return(MchannelProcess::command(argc, argv));
}


void Gplayer::gtrace()
{
    int n = strlen(tmsg_);
    tmsg_[n] = 0;
    
    if (tchan_)
	(void)Tcl_Write(tchan_, tmsg_, n);
    else
	printf("%s", tmsg_);
}
