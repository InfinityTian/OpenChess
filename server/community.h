#ifndef COMMUNITY_H
#define COMMUNITY_H
#include "../src/workspace.h"
/* Authenticated account identity is supplied by the connection, never by JSON. */
cJSON *community_request(Workspace *w,Puzzles *p,int user_id,const char *owner,const cJSON *request);
bool community_blocked(Workspace *w,int a,int b);
#endif
