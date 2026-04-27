#ifndef SERVER_H
#define SERVER_H

int  serverFindPort(int start);
void serverStart(int port, char *argv0, const char *sourcePath);

#endif
