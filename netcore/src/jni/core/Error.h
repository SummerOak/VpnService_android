#ifndef ERROR_H
#define ERROR_H

typedef enum Error{
	SUCCESS,
	DUPLICATE,
	CREATE_EPOLL_FAILED,
	FAILED,
}Error;

#endif