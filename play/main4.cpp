#include <iostream>
#include <random>
#include <chrono>

#define ROW 1000000
#define COL 64

double vals[ROW][COL];

struct timer {
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
	void reset()
	{
		start = std::chrono::high_resolution_clock::now();
	}
	double time()
	{
		return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
	}
	static uint64_t since_epoch()
	{
		return std::chrono::duration_cast<std::chrono::duration<uint64_t, std::ratio<1, 1000000000>>>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	}
};


#if defined(__linux__) || defined(linux) || defined(__linux)
#   // Check the kernel version. `getrandom` is only Linux 3.17 and above.
#   include <linux/version.h>
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
#       define HAVE_GETRANDOM
#   endif
#endif
#if defined(__OpenBSD__)
#   define HAVE_GETENTROPY
#endif

// also requires glibc 2.25 for the libc wrapper
#if defined(HAVE_GETRANDOM)
#   include <sys/syscall.h>
#   include <linux/random.h>

class my_random_device {
public:
	my_random_device()
	{
	}
	~my_random_device()
	{
	}
	uint32_t operator()()
	{
		uint32_t rtn;
		int bytes = syscall(SYS_getrandom, &rtn, 4, 0);
		if (bytes != 4) {
			throw std::runtime_error("Unable to read N bytes from CSPRNG.");
		}
		return rtn;
	}
};

#elif defined(_WIN32)
#include <Windows.h>
#include <exception>
class my_random_device {
public:
	my_random_device()
	{
		if (!CryptAcquireContext(&ctx, nullptr, nullptr, PROV_RSA_FULL, 0)) {
			if (!CryptAcquireContext(&ctx, nullptr, nullptr, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
				throw std::runtime_error("Unable to initialize Win32 crypt library.");
			}
		}
}
	~my_random_device()
	{
		CryptReleaseContext(ctx, 0);
	}
	uint32_t operator()()
	{
		uint32_t rtn;
		if (!CryptGenRandom(ctx, 4, (BYTE*)&rtn)) {
			throw std::runtime_error("Unable to generate random bytes.");
		}
		return rtn;
	}
private:
	HCRYPTPROV ctx;
};


#elif  defined(HAVE_GETENTROPY)

// POSIX sysrandom here.
#include <unistd.h>
class my_random_device {
public:
	my_random_device()
	{}
	~my_random_device()
	{}
	uint32_t operator()()
	{
		uint32_t rtn;
		int bytes = getentropy(&rtn, 4);
		if (bytes != 4) {
			throw std::runtime_error("Unable to read N bytes from CSPRNG.");
		}
		return rtn;
	}
};
#else
#include <fcntl.h>
#include <unistd.h>
class my_random_device {
public:
	my_random_device()
	{}
	~my_random_device()
	{}
	uint32_t operator()()
	{
		int fd = open("/dev/urandom", O_RDONLY);
		if (fd == -1) {
			throw std::runtime_error("Unable to open /dev/urandom.");
		}
		uint32_t rtn;
		if (read(fd, &rtn, 4) != 4) {
			close(fd);
			throw std::runtime_error("Unable to read N bytes from CSPRNG.");
		}
		close(fd);
		return rtn;
	}
};
#endif


void prepare_data()
{
//	std::random_device device;
	my_random_device device;
	std::uint32_t seeds[19937/32+1];
	for (size_t i = 0; i < (19937 / 32 + 1); ++i) {
		seeds[i]= device();
	}
	std::seed_seq seq(seeds,seeds + (19937 / 32 + 1));
	std::mt19937 ran(seq);
	std::uniform_real_distribution<double> dist(0.0,1.0);
	for (int i = 0; i < ROW; ++i) {
		for (int j = 0; j < COL; ++j) {
			vals[i][j] = dist(ran);
		}
	}
}

double sum_row()
{
	double rtn = 0;
	for (int i = 0; i < ROW; ++i) {
		for (int j = 0; j < COL; ++j) {
			rtn += vals[i][j];
		}
	}
	return rtn;
}
double sum_col()
{
	double rtn = 0;
	for (int j = 0; j < COL; ++j) {
		 for (int i = 0; i < ROW; ++i) {
			rtn += vals[i][j];
		}
	}
	return rtn;
}

int main()
{
	prepare_data();
	timer timer;
	for (int i = 0; i < 100; ++i) {
		timer.reset();
		double res = sum_row();
		double time = timer.time();
		printf("row-first result: %.14f\ttime: %.14f\n", res, time);
		timer.reset();
		res = sum_col();
		time = timer.time();
		printf("col-first result: %.14f\ttime: %.14f\n", res, time);
	}
	return 0;
}
