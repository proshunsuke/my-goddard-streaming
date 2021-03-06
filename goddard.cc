/*
 * File:   goddard.cc
 * Author: Jae Chung (goos@cs.wpi.edu)
 * Date:   Jan 2005
 *
 * Goddard server implementation
 */

#include "goddard.h"
#include "random.h"

static class GoddardClass : public TclClass {
public:
    GoddardClass() : TclClass("Process/Mchannel/Goddard") {}
    TclObject* create(int argc, const char*const* argv) {
        return (new Goddard());
    }
} class_goddard_process;


// When send_timer_ expires call Goddard:send_frame()
void FrameSendTimer::expire(Event*)
{
    t_->send_frame();
}


Goddard::Goddard() : MchannelProcess(), send_timer_(this)
{
    bind("max_scale_", &gddp_.maxscale);
    bind("frm_size_0_", &gddp_.frmsize[0]);
    bind("frm_size_1_", &gddp_.frmsize[1]);
    bind("frm_size_2_", &gddp_.frmsize[2]);
    bind("frm_size_3_", &gddp_.frmsize[3]);
    bind("frm_size_4_", &gddp_.frmsize[4]);
    bind("frm_size_5_", &gddp_.frmsize[5]);
    bind("frm_size_6_", &gddp_.frmsize[6]);
    bind("frm_interval_0_", &gddp_.interval[0]);
    bind("frm_interval_1_", &gddp_.interval[1]);
    bind("frm_interval_2_", &gddp_.interval[2]);
    bind("frm_interval_3_", &gddp_.interval[3]);
    bind("frm_interval_4_", &gddp_.interval[4]);
    bind("frm_interval_5_", &gddp_.interval[5]);
    bind("frm_interval_6_", &gddp_.interval[6]);
    bind_bool("random_", &gddp_.random);
    bind("fragment_size_", &gddp_.fragsize);

    gddv_.state = STOPPED;
}

void Goddard::init()
{
    if(target_[CNTRL_CH] == NULL || ctype_[CNTRL_CH] != PT_TCP) {
        fprintf(stderr, "Goddard(%s): CNTRL channel error\n", this->name());
        abort();
    }
    if(target_[MEDIA_CH] == NULL) {
        fprintf(stderr, "Goddard(%s): MEDIA channel error\n", this->name());
        abort();
    }
    if(target_[PPAIR_CH] == NULL || ctype_[PPAIR_CH] != PT_UDP) {
        fprintf(stderr, "Goddard(%s): PPAIR channel error\n", this->name());
        abort();
    }

    gddv_.state = STOPPED;
    gddv_.scale = gddp_.maxscale - 1;
    gddv_.exp_cseq = 0;
    gddv_.last_mseq = -1;
    gddv_.last_cseq = -1;
    gddv_.last_pseq = -1;
    gddv_.buf_factor = 1.0;
}

void Goddard::start()
{
    char buf[MAX_MSGLEN];

    init();

    // Send the number of scale levels
    sprintf(buf, "MAXL %d\0", gddp_.maxscale);
    send_ctrmsg(buf);

    // Send each scale level bitrate
    for(int i=0; i<gddp_.maxscale; i++) {
        sprintf(buf, "RATE %d %f %d\0", i,
                gddp_.frmsize[i]/gddp_.interval[i],
                (int)(1/gddp_.interval[i]));
        send_ctrmsg(buf);
    }

    // Send gplayer ready message
    send_ctrmsg("READY");

    // Gplayer, when getting "READY" message from Goddard server, will request
    // a packet pair (via control channel).  After estimating the bottleneck
    // bandwidth using packet pairs, Gplayer will send "PLAY" control message
    // to Goddard (starts streaming).
}


void Goddard::stop()
{
    gddv_.state = STOPPED;

    //send gplayer a stop msg
    send_ctrmsg("STOP");
}

void Goddard::send_ctrmsg(const char *msg)
{
    if (strlen(msg) > MAX_MSGLEN) {
        fprintf(stderr, "Goddard: strlen(ctr_msg) > %d\n", MAX_MSGLEN);
        abort();
    }

    struct goddard_frame frm;
    frm.type = CNTRL_DATA;
    frm.size = sizeof(frm);
    frm.seq = ++gddv_.last_cseq;
    strcpy(frm.u.ctr.msg, msg);

    TcpAppString* data = new TcpAppString();
    data->set_buffer((void*)&frm, sizeof(frm));
    send_data(CNTRL_CH, frm.size, data);
}

void Goddard::send_pktpair()
{
    struct goddard_frame frm;
    frm.type = PPAIR_DATA;
    frm.size = gddp_.fragsize;

    for(int i=0; i<2; i++) {
        frm.seq = ++gddv_.last_pseq;
        PacketData* data = new PacketData(sizeof(frm));
        memcpy((void*)data->data(), (void*)&frm, sizeof(frm));
        send_data(PPAIR_CH, frm.size, data);
    }
}

void Goddard::send_frame()
{
    if (gddv_.state != STOPPED) {
        struct goddard_frame frm;
        frm.type = MEDIA_DATA;
        frm.size = gddp_.frmsize[gddv_.scale];
        frm.seq = ++gddv_.last_mseq;
        frm.u.med.seg_last = 0;
        frm.u.med.seg_cur = 0;
        frm.u.med.interval = gddp_.interval[gddv_.scale];

        if(ctype_[MEDIA_CH] == PT_TCP) { /* TCP channel */
            TcpAppString* data = new TcpAppString();
            data->set_buffer((void*)&frm, sizeof(frm));
            send_data(MEDIA_CH, frm.size, data);
        }
        else { /* Other channels */
            int num_frag   = frm.size / gddp_.fragsize;
            int xtra_bytes = frm.size % gddp_.fragsize;
            frm.size = gddp_.fragsize;
            frm.u.med.seg_last = num_frag;
            if(!xtra_bytes) frm.u.med.seg_last--;
            while(num_frag--) {
                PacketData* data = new PacketData(sizeof(frm));
                memcpy((void*)data->data(), (void*)&frm, sizeof(frm));
                send_data(MEDIA_CH, frm.size, data);
                frm.u.med.seg_cur++;
            }
            if(xtra_bytes) {
                frm.size = xtra_bytes;
                PacketData* data = new PacketData(sizeof(frm));
                memcpy((void*)data->data(), (void*)&frm, sizeof(frm));
                send_data(MEDIA_CH, frm.size, data);
            }
        }
        // Reschedule the send_pkt timer
        double next_time = next_send_time();
        if(next_time > 0) send_timer_.resched(next_time);
    }
}

// Schedule next frame transmission time
double Goddard::next_send_time()
{
    double next_time = gddp_.interval[gddv_.scale];
    if(gddv_.state == BUFFERING)
        next_time /= gddv_.buf_factor;
    if(gddp_.random)
        next_time += next_time * Random::uniform(-0.1, 0.1);

    return next_time;
}


void Goddard::process_data(int size, AppData* data)
{
    struct goddard_frame *frm;

    if (data == NULL)
        return;
    if (data->type() == TCPAPP_STRING)
        frm = (struct goddard_frame*)((TcpAppString*)data)->str();
    else if (data->type() == PACKET_DATA)
        frm = (struct goddard_frame*)((PacketData*)data)->data();
    else {
        fprintf(stderr, "Goddard: unknown data type received\n");
        abort();
    }

    // Only control message is expected (from a gplayer)
    if (frm->type == CNTRL_DATA) process_ctrmsg(frm);
    else {
        fprintf(stderr, "Goddard: unknown frame type received\n");
        abort();
    }
}

void Goddard::process_ctrmsg(struct goddard_frame *frm)
{
    //printf("%f Goddard %s got ctr-frame seq: %d size: %d msg: %s\n",
    //     Scheduler::instance().clock(), this->name(),
    //     frm->seq, frm->size, frm->u.ctr.msg);

    int argc; char *argv[5];
    argv[0] = strtok(frm->u.ctr.msg, " ");
    for (argc=1; argc<5; argc++) {
        argv[argc] = strtok(NULL, " ");
        if (argv[argc] == NULL) break;
    }


    if (strncmp(argv[0], "PKTP", 4) == 0) {
        // Send packet pair
        send_pktpair();
    }
    else if (strncmp(argv[0], "PLAY", 4) == 0) {
        //start streaming
        gddv_.state = BUFFERING;
        send_frame();
    }
    else if (strncmp(argv[0], "BUFF", 4) == 0) {
        //rebuffering event
        gddv_.state = BUFFERING;
    }
    else if (strncmp(argv[0], "STRM", 4) == 0) {
        //buffering is over
        gddv_.state = STREAMING;
    }
    else if (strncmp(argv[0], "BFCT", 4) == 0 && argc == 2) {
        // Send packet pair
        gddv_.buf_factor = atof(argv[1]);
        if (gddv_.buf_factor < 0) {
            fprintf(stderr, "Goddard: buf_factor is out of range.\n");
            abort();
        }
    }
    else if (strncmp(argv[0], "SSET", 4) == 0 && argc == 2) {
        //set scale to given value
        gddv_.scale = atoi(argv[1]);
        if (gddv_.scale < 0 || gddv_.scale >= gddp_.maxscale) {
            fprintf(stderr, "Goddard: scale value (%d) out of range.\n",
                    gddv_.scale);
            abort();
        }
    }
    else {
        fprintf(stderr, "Goddard: unknown control message: \n", argv[0]);
        abort();
    }
}


int Goddard::command(int argc, const char*const* argv)
{
        Tcl& tcl = Tcl::instance();

        if (argc == 2) {
                if (strcmp(argv[1], "start") == 0) {
                        // enableRecv_ only if recv() exists in Tcl
                        tcl.evalf("[%s info class] info instprocs", name_);
                        char result[1024];
                        sprintf(result, " %s ", tcl.result());
                        //enableRecv_ = (strstr(result, " recv ") != 0);
                        //enableResume_ = (strstr(result, " resume ") != 0);
                        start();
                        return (TCL_OK);
                }
                if (strcmp(argv[1], "stop") == 0) {
                        stop();
                        return (TCL_OK);
                }
        }

        return(MchannelProcess::command(argc, argv));
}
