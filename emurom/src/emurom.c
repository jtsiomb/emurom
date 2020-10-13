#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

enum { FMT_BIN, FMT_IHEX, FMT_SREC };

int send_bin(FILE *infile, int dev);
int send_ihex(FILE *infile, int dev);
int send_srec(FILE *infile, int dev);
void print_usage(const char *argv0);

FILE *infile;
int sdev;
int base_addr;

int main(int argc, char **argv)
{
	int i;
	char *endp;
	const char *serdev = "/dev/ttyACM0";
	const char *fname = 0;
	struct termios term;
	int (*send)(FILE*, int) = send_bin;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(argv[i][2] == 0) {
				switch(argv[i][1]) {
				case 'd':
					if(!argv[++i]) {
						fprintf(stderr, "-d must be followed by a device file\n");
						return 1;
					}
					serdev = argv[i];
					break;

				case 'f':
					if(strcmp(argv[++i], "bin") == 0) {
						send = send_bin;
					} else if(strcmp(argv[i], "ihex") == 0 || strcmp(argv[i], "hex") == 0) {
						send = send_ihex;
					} else if(strcmp(argv[i], "srec") == 0) {
						send = send_srec;
					} else {
						fprintf(stderr, "invalid input format: %s\n", argv[i]);
						return 1;
					}
					break;

				case 'a':
					base_addr = strtol(argv[++i], &endp, 0);
					if(endp == argv[i]) {
						fprintf(stderr, "-a must be followed by a base address offset\n");
						return 1;
					}
					break;

				case 'h':
					print_usage(argv[0]);
					return 0;

				default:
					fprintf(stderr, "invalid option: %s\n", argv[i]);
					print_usage(argv[0]);
					return 1;
				}
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				print_usage(argv[0]);
				return 1;
			}
		} else {
			if(fname) {
				fprintf(stderr, "unexpected argument: %s\n", argv[i]);
				print_usage(argv[0]);
				return 1;
			}
			fname = argv[i];
		}
	}

	if(fname) {
		if(!(infile = fopen(fname, "rb"))) {
			fprintf(stderr, "failed to open input file: %s: %s\n", fname, strerror(errno));
			return 1;
		}
	} else {
		infile = stdin;
	}

	if((sdev = open(serdev, O_RDWR | O_NOCTTY)) == -1) {
		fprintf(stderr, "failed to connect to devrom board (%s): %s\n", serdev, strerror(errno));
		return 1;
	}
	tcgetattr(sdev, &term);

	term.c_oflag = 0;
	term.c_lflag = 0;
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;

	term.c_cflag = CLOCAL | CREAD | CS8 | HUPCL;
	term.c_iflag = IGNBRK | IGNPAR;

	cfsetispeed(&term, B38400);
	cfsetospeed(&term, B38400);

	if(tcsetattr(sdev, TCSANOW, &term) < 0) {
		fprintf(stderr, "failed to set terminal attributes\n");
		return 1;
	}

	if(send(infile, sdev) == -1) {
		return 1;
	}

	fclose(infile);
	return 0;
}

int read_line(int fd, char *buf, int bufsz)
{
	int i, num;
	bufsz--;	/* leave space for the terminator */

	while(bufsz > 0) {
		if((num = read(fd, buf, bufsz)) <= 0) {
			break;
		}

		for(i=0; i<num; i++) {
			if(*buf == '\r' || *buf == '\n') {
				*buf = 0;
				return 0;
			}
			buf++;
		}

		bufsz -= num;
		*buf = 0;
	}

	return -1;
}

int read_status(int dev)
{
	char buf[128];

	if(read_line(dev, buf, sizeof buf) == -1) {
		fprintf(stderr, "failed to read status\n");
		return -1;
	}
	buf[127] = 0;

	if(memcmp(buf, "ERR", 3) == 0) {
		fprintf(stderr, "error:%s\n", buf + 3);
		return -1;
	}
	return 0;
}

int cmd(int dev, const char *cmd)
{
	write(dev, cmd, strlen(cmd));
	if(read_status(dev) != 0) {
		return -1;
	}
	return 0;
}

int cmd_addr(int dev, int addr)
{
	char buf[32];
	sprintf(buf, "a %d\n", addr);
	return cmd(dev, buf);
}

int cmd_write(int dev, int val)
{
	char buf[32];

	if(val < 0 || val >= 256) {
		fprintf(stderr, "cmd_write: invalid byte value: %d\n", val);
		return -1;
	}

	sprintf(buf, "w %d\n", val);
	return cmd(dev, buf);
}

int send_bin(FILE *infile, int dev)
{
	int c, res = -1;
	long count = 0, fsz = -1;

	if(fseek(infile, 0, SEEK_END) != -1) {
		fsz = ftell(infile);
		rewind(infile);
	}

	if(cmd(dev, "p\n") == -1 || cmd_addr(dev, 0) == -1) {
		return -1;
	}

	while((c = fgetc(infile)) != -1) {
		if(cmd_write(dev, c) == -1) {
			goto end;
		}

		if(fsz > 0) {
			printf("\r%ld/%ld            ", ++count, fsz);
			fflush(stdout);
		}
	}
	res = 0;

end:
	if(fsz > 0) putchar('\n');
	cmd(dev, "b\n");
	return res;
}

long hexval(const char *s, int digits)
{
	int i;
	long val = 0;

	for(i=0; i<digits; i++) {
		if(!isxdigit(*s)) {
			return -1;
		}
		val <<= 4;

		if(*s >= 'a') {
			val |= *s - 'a' + 10;
		} else if(*s >= 'A') {
			val |= *s - 'A' + 10;
		} else {
			val |= *s - '0';
		}
		s++;
	}
	return val;
}

const char *substr(const char *s, int len)
{
	char *tmp;
	static char *buf;
	static int buflen;

	if(len > buflen) {
		if(!(tmp = malloc(len + 1))) {
			perror("failed to allocate substring buffer");
			return 0;
		}
		free(buf);
		buf = tmp;
		buflen = len;
	}
	memcpy(buf, s, len);
	buf[len] = 0;
	return buf;
}

enum {
	TYPE_DATA		= 0,
	TYPE_EOF		= 1,
	TYPE_EXT16		= 2,
	TYPE_START16	= 3,
	TYPE_EXT32		= 4,
	TYPE_START32	= 5
};

int send_ihex(FILE *infile, int dev)
{
	char line[512], *ptr;
	int res = -1, count, addr, type, val;
	int cur_addr = -1, offs = 0;

	if(cmd(dev, "p\n") == -1) {
		return -1;
	}

	while(fgets(line, sizeof line, infile)) {
		if(line[0] != ':') continue;

		ptr = line + 1;

		if((count = hexval(ptr, 2)) == -1) {
			fprintf(stderr, "ihex: invalid count field: %s\n", substr(ptr, 2));
			goto end;
		}
		ptr += 2;
		if((addr = hexval(ptr, 4)) == -1) {
			fprintf(stderr, "ihex: invalid address field: %s\n", substr(ptr, 4));
			goto end;
		}
		ptr += 4;
		if((type = hexval(ptr, 2)) == -1 || type > 5) {
			fprintf(stderr, "ihex: invalid type field: %s\n", substr(ptr, 2));
			goto end;
		}
		ptr += 2;

		switch(type) {
		case TYPE_DATA:
			addr += base_addr + offs;
			if(cur_addr != addr) {
				cur_addr = addr;
				if(cmd_addr(dev, addr) == -1) {
					goto end;
				}
			}

			cur_addr += count;
			while(count-- > 0) {
				if((val = hexval(ptr, 2)) == -1) {
					fprintf(stderr, "ihex: invalid value: %s\n", substr(ptr, 2));
					goto end;
				}
				if(cmd_write(dev, val) == -1) {
					goto end;
				}
				ptr += 2;
			}
			break;

		case TYPE_EOF:
			res = 0;
			goto end;

		case TYPE_EXT16:
			if((val = hexval(ptr, 4)) == -1) {
				fprintf(stderr, "ihex: invalid extended segment address: %s\n", substr(ptr, 4));
				goto end;
			}
			offs = val << 4;
			ptr += 4;
			break;

		case TYPE_EXT32:
			if((val = hexval(ptr, 8)) == -1) {
				fprintf(stderr, "ihex: invalid extended linear address: %s\n", substr(ptr, 8));
				goto end;
			}
			offs = val;
			ptr += 8;
			break;

		default:
			break;
		}

	}
	res = 0;

end:
	cmd(dev, "b\n");
	return res;
}

int send_srec(FILE *infile, int dev)
{
	fprintf(stderr, "TODO: SREC format not implemented yet\n");
	return -1;
}

void print_usage(const char *argv0)
{
	printf("Usage: %s [options] [input file]\n", argv0);
	printf("If no input file is specified, defaults to reading standard input\n");
	printf("Options:\n");
	printf("  -d <device file> devrom board USB serial port (default: /dev/ttyACM0)\n");
	printf("  -f <input format> format of the input file [bin/ihex/srec] (default: bin)\n");
	printf("  -a <base address> address offset (default: 0)\n");
	printf("  -h print usage and exit\n");
}
