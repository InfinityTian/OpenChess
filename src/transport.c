#include "transport.h"

int transport_poll(Transport *t, char *line, size_t cap)
{
    if (!t || !t->ops || !t->ops->poll) return -1;
    return t->ops->poll(t, line, cap);
}

bool transport_send(Transport *t, const char *line)
{
    if (!t || !t->ops || !t->ops->send) return false;
    return t->ops->send(t, line);
}

void transport_close(Transport *t)
{
    if (!t || !t->ops || !t->ops->close) return;
    t->ops->close(t);
}

NetState transport_state(const Transport *t)
{
    if (!t || !t->ops || !t->ops->state) return NET_STATE_NONE;
    return t->ops->state(t);
}

NetRole transport_role(const Transport *t)
{
    if (!t || !t->ops || !t->ops->role) return NET_ROLE_NONE;
    return t->ops->role(t);
}
