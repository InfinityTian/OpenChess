/* Manual-signaling WebRTC: no account, rendezvous or game server. */
#include "net.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef HAVE_P2P
#define HAVE_P2P 0
#endif
#if HAVE_P2P
#include <SDL.h>
#include <rtc/rtc.h>
#define QUEUE 128
#define MESSAGE 1024
typedef struct {
    Transport base;
    SDL_mutex *lock;
    int pc, dc;
    NetRole role;
    NetState state;
    bool gathered, remote, closing;
    unsigned head, count;
    char messages[QUEUE][MESSAGE];
} Peer;
static void state_cb(int id, rtcState state, void *ptr)
{
    (void)id; Peer *p=ptr; SDL_LockMutex(p->lock);
    if(state==RTC_FAILED||state==RTC_CLOSED)p->state=NET_STATE_CLOSED;
    SDL_UnlockMutex(p->lock);
}
static void gather_cb(int id, rtcGatheringState state, void *ptr)
{
    (void)id; Peer *p=ptr; SDL_LockMutex(p->lock);
    p->gathered=state==RTC_GATHERING_COMPLETE; SDL_UnlockMutex(p->lock);
}
static void open_cb(int id, void *ptr)
{
    (void)id; Peer *p=ptr; SDL_LockMutex(p->lock);
    if(!p->closing)p->state=NET_STATE_CONNECTED; SDL_UnlockMutex(p->lock);
}
static void closed_cb(int id, void *ptr)
{
    (void)id; Peer *p=ptr; SDL_LockMutex(p->lock);
    p->state=NET_STATE_CLOSED; SDL_UnlockMutex(p->lock);
}
static void message_cb(int id,const char *message,int size,void *ptr)
{
    (void)id; Peer *p=ptr;
    /* Accept text only. Never truncate a protocol message. */
    if(size>=0)return;
    size_t n=strlen(message);
    SDL_LockMutex(p->lock);
    if(n>=MESSAGE||p->count==QUEUE||strchr(message,'\n')||strchr(message,'\r'))p->state=NET_STATE_CLOSED;
    else {snprintf(p->messages[(p->head+p->count)%QUEUE],MESSAGE,"%s",message);p->count++;}
    SDL_UnlockMutex(p->lock);
}
static void channel_cb(int pc,int dc,void *ptr)
{
    (void)pc; Peer *p=ptr; SDL_LockMutex(p->lock);
    if(p->closing||p->dc>=0){SDL_UnlockMutex(p->lock);rtcDeleteDataChannel(dc);return;}
    p->dc=dc; SDL_UnlockMutex(p->lock);
    rtcSetUserPointer(dc,p);rtcSetOpenCallback(dc,open_cb);
    rtcSetClosedCallback(dc,closed_cb);rtcSetMessageCallback(dc,message_cb);
}
static int peer_poll(Transport *t,char *line,size_t cap)
{
    Peer *p=(Peer *)t; int result=0; SDL_LockMutex(p->lock);
    if(p->state==NET_STATE_CLOSED)result=-1;
    else if(p->count){
        size_t n=strlen(p->messages[p->head]);
        if(n+1>cap){p->state=NET_STATE_CLOSED;result=-1;}
        else {memcpy(line,p->messages[p->head],n+1);p->head=(p->head+1)%QUEUE;p->count--;result=1;}
    }
    SDL_UnlockMutex(p->lock);return result;
}
static NetState peer_state(const Transport *t)
{
    Peer *p=(Peer *)t; SDL_LockMutex(p->lock);NetState s=p->state;SDL_UnlockMutex(p->lock);return s;
}
static NetRole peer_role(const Transport *t){return ((const Peer *)t)->role;}
static bool peer_send(Transport *t,const char *line)
{
    Peer *p=(Peer *)t;
    if(!line||strlen(line)>=MESSAGE||strchr(line,'\n')||strchr(line,'\r')||peer_state(t)!=NET_STATE_CONNECTED)return false;
    return rtcSendMessage(p->dc,line,-1)>=0;
}
static void peer_close(Transport *t)
{
    Peer *p=(Peer *)t;SDL_LockMutex(p->lock);p->closing=true;SDL_UnlockMutex(p->lock);
    /* Deletion waits for callbacks; never hold our mutex while deleting. */
    rtcDeletePeerConnection(p->pc);
    if(p->dc>=0)rtcDeleteDataChannel(p->dc);
    SDL_DestroyMutex(p->lock);free(p);
}
static const TransportOps ops={peer_poll,peer_send,peer_close,peer_state,peer_role};
bool net_p2p_available(void){return true;}
bool net_is_p2p(const Transport *t){return t&&t->ops==&ops;}
Transport *net_p2p_create(bool host,const char *ice)
{
    Peer *p=calloc(1,sizeof *p);if(!p)return NULL;
    p->base.ops=&ops;p->pc=p->dc=-1;p->role=host?NET_ROLE_HOST:NET_ROLE_JOIN;p->state=NET_STATE_CONNECTING;
    p->lock=SDL_CreateMutex();if(!p->lock){free(p);return NULL;}
    const char *servers[]={ice};rtcConfiguration cfg={0};
    if(ice&&*ice){cfg.iceServers=servers;cfg.iceServersCount=1;}
    cfg.disableAutoNegotiation=true;cfg.maxMessageSize=MESSAGE;
    p->pc=rtcCreatePeerConnection(&cfg);
    if(p->pc<0){SDL_DestroyMutex(p->lock);free(p);return NULL;}
    rtcSetUserPointer(p->pc,p);rtcSetStateChangeCallback(p->pc,state_cb);
    rtcSetGatheringStateChangeCallback(p->pc,gather_cb);rtcSetDataChannelCallback(p->pc,channel_cb);
    if(host){
        int dc=rtcCreateDataChannel(p->pc,"openchess-v1");
        if(dc<0){peer_close(&p->base);return NULL;}
        channel_cb(p->pc,dc,p);
        if(rtcSetLocalDescription(p->pc,"offer")<0){peer_close(&p->base);return NULL;}
    }
    return &p->base;
}
bool net_p2p_export(Transport *t,char *text,size_t cap)
{
    if(!net_is_p2p(t)||!text||cap<1)return false;Peer *p=(Peer *)t;
    SDL_LockMutex(p->lock);bool ready=p->gathered;SDL_UnlockMutex(p->lock);if(!ready)return false;
    char sdp[32768];if(rtcGetLocalDescription(p->pc,sdp,sizeof sdp)<0)return false;
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"type",p->role==NET_ROLE_HOST?"offer":"answer");
    cJSON_AddStringToObject(j,"sdp",sdp);char *json=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!json)return false;size_t n=strlen(json);bool ok=n+10<cap;
    if(ok)snprintf(text,cap,"OC-P2P1:%s",json);free(json);return ok;
}
bool net_p2p_import(Transport *t,const char *text)
{
    if(!net_is_p2p(t)||!text||strlen(text)>65535||strncmp(text,"OC-P2P1:",8))return false;
    Peer *p=(Peer *)t;if(p->remote)return false;
    const char *end=NULL;cJSON *j=cJSON_ParseWithOpts(text+8,&end,true);if(!j)return false;
    const cJSON *type=cJSON_GetObjectItemCaseSensitive(j,"type"),*sdp=cJSON_GetObjectItemCaseSensitive(j,"sdp");
    bool ok=cJSON_IsString(type)&&cJSON_IsString(sdp)&&strlen(sdp->valuestring)<32768&&
        !strcmp(type->valuestring,p->role==NET_ROLE_HOST?"answer":"offer")&&
        strstr(sdp->valuestring,"a=fingerprint:")&&strstr(sdp->valuestring,"a=candidate:");
    if(ok)ok=rtcSetRemoteDescription(p->pc,sdp->valuestring,type->valuestring)>=0;
    if(ok){p->remote=true;if(p->role==NET_ROLE_JOIN)ok=rtcSetLocalDescription(p->pc,"answer")>=0;}
    cJSON_Delete(j);return ok;
}
#else
bool net_p2p_available(void){return false;}
bool net_is_p2p(const Transport *t){(void)t;return false;}
Transport *net_p2p_create(bool host,const char *ice){(void)host;(void)ice;return NULL;}
bool net_p2p_export(Transport *t,char *text,size_t cap){(void)t;(void)text;(void)cap;return false;}
bool net_p2p_import(Transport *t,const char *text){(void)t;(void)text;return false;}
#endif
