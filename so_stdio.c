#include "so_stdio.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DIM_BUFFER 4096

struct _so_file {
	int _fd;
	int _mode;
	int _offsetFile;
	int _offsetBuffer;
	int _bytesRead;
	int _writeFlag;
	int _error;
	int _eof;
	pid_t _childPid;
	unsigned char _buffer[DIM_BUFFER];
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *stream = (SO_FILE *)malloc(sizeof(SO_FILE));

	if  (stream == NULL) {
		stream->_error = -1;
		return NULL;
	}

	if (strcmp(mode, "r") == 0) {
		stream->_fd = open(pathname, O_RDONLY);
		stream->_mode = 1;
	} else if (strcmp(mode, "r+") == 0) {
		stream->_fd = open(pathname, O_RDWR);
		stream->_mode = 2;
	} else if (strcmp(mode, "w") == 0) {
		stream->_fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		stream->_mode = 3;
	} else if (strcmp(mode, "w+") == 0) {
		stream->_fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		stream->_mode = 4;
	} else if (strcmp(mode, "a") == 0) {
		stream->_fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644);
		stream->_mode = 5;
	} else if (strcmp(mode, "a+") == 0) {
		stream->_fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
		stream->_mode = 6;
	} else {
		stream->_error = -1;
		free(stream);
		return NULL;
	}

	if (stream->_fd == -1) {
		stream->_error = -1;
		free(stream);
		return NULL;
	}

	for (int i = 0; i < DIM_BUFFER; i++)
		stream->_buffer[i] = '\0';

	stream->_error = 0;
	stream->_offsetBuffer = 0;
	stream->_offsetFile = 0;
	stream->_bytesRead = 0;
	stream->_writeFlag = -1;
	stream->_eof = 0;
	stream->_childPid = -1;

	return stream;
}

int so_fclose(SO_FILE *stream)
{
	if (stream == NULL) {
		stream->_error = -1;
		return SO_EOF;
	}

	if (stream->_writeFlag == 1) {
		if (so_fflush(stream) == SO_EOF) {
			stream->_error = -1;
			free(stream);
			return SO_EOF;
		}
	}

	int res = close(stream->_fd);

	if (res == -1) {
		stream->_error = -1;
		free(stream);
		return SO_EOF;
	}

	free(stream);
	return 0;
}

int so_fileno(SO_FILE *stream)
{
	if (stream->_fd != -1)
		return stream->_fd;
	stream->_error = -1;
	return -1;
}

int so_fflush(SO_FILE *stream)
{
	if (stream == NULL || stream->_fd < 0)
		return SO_EOF;

	if (stream->_mode == 5 || stream->_mode == 6)
		lseek(stream->_fd, 0, SEEK_END);

	if (stream->_mode == 1) {
		for (int i = 0; i < stream->_offsetBuffer; i++)
			stream->_buffer[i] = '\0';

		stream->_offsetBuffer = 0;
		return 0;
	}

	if (stream->_writeFlag == 1) {
		int bytesWritten = 0;

		while (bytesWritten != stream->_offsetBuffer) {
			int res = write(stream->_fd, stream->_buffer+bytesWritten, stream->_offsetBuffer-bytesWritten);

			if (res == -1) {
				stream->_error = -1;
				return SO_EOF;
			}
		bytesWritten += res;
		}
		stream->_writeFlag = 0;
	}

	for (int i = 0; i < stream->_offsetBuffer; i++)
		stream->_buffer[i] = '\0';

	stream->_offsetBuffer = 0;

	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream == NULL) {
		stream->_error = -1;
		return SO_EOF;
	}
	if (so_fflush(stream) == SO_EOF) {
		stream->_error = -1;
		return SO_EOF;
	}
	stream->_offsetFile = lseek(stream->_fd, offset, whence);

	if (stream->_offsetFile < 0) {
		stream->_error = -1;
		return SO_EOF;
	}
	return 0;
}

long so_ftell(SO_FILE *stream)
{
	if (stream->_fd != -1 && stream->_offsetFile >= 0)
		return stream->_offsetFile;
	else
		return SO_EOF;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (size == 0 || nmemb == 0) {
		stream->_error = -1;
		return SO_EOF;
	}

	if (stream->_writeFlag == -1)
		stream->_writeFlag = 0;

	int bytes_read = 0;

	for (size_t i = 0; i < nmemb * size; i++) {
		if (so_feof(stream) == 1) {
			stream->_error = -1;
			return SO_EOF;
		}
		int ch = so_fgetc(stream);

		if (ch == SO_EOF) {
			if (bytes_read == 0)
				return 0;

			return bytes_read / size;
		}

		((unsigned char *)ptr)[bytes_read] = (unsigned char)ch;
		bytes_read++;
	}

	return bytes_read / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (size == 0 || nmemb == 0) {
		stream->_error = -1;
		return 0;
	}
	if (stream->_mode == 1) {
		stream->_error = -1;
		return 0;
	}

	int bytes_written = 0;

	for (int i = 0; i < nmemb * size; i++) {
		int ch = so_fputc((int)(((unsigned char *)ptr)[bytes_written]), stream);

		if (ch == SO_EOF) {
			stream->_error = -1;
			return bytes_written / size;
		}
		bytes_written++;
	}

	return bytes_written / size;
}

int so_fgetc(SO_FILE *stream)
{
	if (stream->_mode == 3 || stream->_mode == 5) {
		stream->_error = -1;
		return SO_EOF;
	}

	if (stream->_writeFlag == 1) {
		if (so_fflush(stream) != 0)
			return SO_EOF;
	}
	stream->_writeFlag = 0;

	if (stream->_offsetBuffer == 0 || stream->_bytesRead == stream->_offsetBuffer) {
		stream->_bytesRead = read(stream->_fd, stream->_buffer, DIM_BUFFER);
		if (stream->_bytesRead == -1) {
			stream->_error = -1;
			return SO_EOF;
		}
		if (stream->_bytesRead == 0) {  // eof
			stream->_eof = 1;
			return SO_EOF;
		}
		stream->_offsetBuffer = 0;
	}

	unsigned char c = stream->_buffer[stream->_offsetBuffer];

	stream->_offsetBuffer++;
	stream->_offsetFile++;
	return (int)c;
}

int so_fputc(int c, SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;

	if (stream->_mode == 1) {
		stream->_error = -1;
		return SO_EOF;
	}

	if (stream->_offsetBuffer == DIM_BUFFER) {
		if (so_fflush(stream) != 0) {
			stream->_error = -1;
			return SO_EOF;
		}
	}

	stream->_writeFlag = 1;

	stream->_buffer[stream->_offsetBuffer] = (char)c;
	stream->_offsetBuffer++;
	stream->_offsetFile++;

	return c;
}

int so_feof(SO_FILE *stream)
{
	return stream->_eof;
}

int so_ferror(SO_FILE *stream)
{
	return stream->_error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *stream = (SO_FILE *)malloc(sizeof(SO_FILE));

	if (stream == NULL)
		return NULL;

	pid_t pid;
	int pipe_fd[2];

	if (pipe(pipe_fd) == -1) {
		free(stream);
		return NULL;
	}

	if (strcmp(type, "r") == 0) {
		stream->_mode = 1;
		stream->_fd = pipe_fd[0];
	} else if (strcmp(type, "w") == 0) {
		stream->_mode = 3;
		stream->_fd = pipe_fd[1];
	} else {
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		free(stream);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		free(stream);
		return NULL;
	}

	if (pid == 0) {
		if (strcmp(type, "r") == 0) {
			close(pipe_fd[0]);
			dup2(pipe_fd[1], STDOUT_FILENO);
		} else if (strcmp(type, "w") == 0) {
			close(pipe_fd[1]);
			dup2(pipe_fd[0], STDIN_FILENO);
		} else {
			close(pipe_fd[0]);
			close(pipe_fd[1]);
			free(stream);
			return NULL;
		}

		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		free(stream);
		return NULL;
	}
	if (pid > 0) {
		for (int i = 0; i < DIM_BUFFER; i++)
			stream->_buffer[i] = '\0';

		stream->_error = 0;
		stream->_offsetBuffer = 0;
		stream->_offsetFile = 0;
		stream->_writeFlag = -1;
		stream->_bytesRead = 0;
		stream->_eof = 0;
		stream->_childPid = pid;

		if (strcmp(type, "r") == 0)
			close(pipe_fd[1]);
		else
			close(pipe_fd[0]);
	}
	return stream;
}

int so_pclose(SO_FILE *stream)
{
	if (stream == NULL)
		return -1;

	int pid = stream->_childPid;
	int fd = stream->_fd;

	if (so_fflush(stream) == SO_EOF) {
		stream->_error = -1;
		return SO_EOF;
	}

	free(stream);

	if (close(fd) == -1)
		return SO_EOF;

	int status;
	int res = waitpid(pid, &status, 0);

	if (res == -1)
		return SO_EOF;

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return SO_EOF;
}
