#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <cerrno>

class Mutex {
public:
	Mutex() {pthread_mutex_init (&m_, nullptr);}
	~Mutex() {pthread_mutex_destroy(&m_);}
	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;

	void lock() {pthread_mutex_lock(&m_);}
	void unlock() {pthread_mutex_unlock(&m_);}
	pthread_mutex_t* native() {return  &m_;}
private:
	pthread_mutex_t m_;
};

class Guard {
public:
	explicit Guard(Mutex& m) : m_(m) {m_.lock();}
	~Guard() {m_.unlock();}
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;
private:
	Mutex& m_;
};

class Cond {
public:
	 Cond() {
		pthread_condattr_t a;
		pthread_condattr_init(&a);
		pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
        	pthread_cond_init(&c_, &a);
        	pthread_condattr_destroy(&a);
    	}
	~Cond() {pthread_cond_destroy(&c_);}
	Cond(const Cond&) = delete;
	Cond& operator=(const Cond&) = delete;

	void wait(Mutex& m) {pthread_cond_wait(&c_, m.native());}
	bool wait_until(Mutex& m, const timespec& dl) {
		return pthread_cond_timedwait(&c_, m.native(), &dl) != ETIMEDOUT;
	}
	bool wait_ms(Mutex& m, long ms) {return wait_until(m, deadline_ms(ms));}
	void signal() {pthread_cond_signal(&c_);}
    	void broadcast() { pthread_cond_broadcast(&c_); }

    	static timespec deadline_ms(long ms) {
        	timespec ts;
        	clock_gettime(CLOCK_MONOTONIC, &ts);
        	ts.tv_sec  += ms / 1000;
        	ts.tv_nsec += (ms % 1000) * 1000000L;
        	if (ts.tv_nsec >= 1000000000L) {ts.tv_sec += 1; ts.tv_nsec -= 1000000000L;}
        	return ts;
    	}
private:
    	pthread_cond_t c_;
};

class Sem {
public:
    	Sem() : ok_(false) {}
    	~Sem() {if (ok_) sem_destroy(&s_);}
    	Sem(const Sem&) = delete;
    	Sem& operator=(const Sem&) = delete;

    	void init(unsigned n) {sem_init(&s_, 0, n); ok_ = true;}
    	void wait() {while (sem_wait(&s_) == -1 && errno == EINTR) {}}
    	void post() {sem_post(&s_); }
private:
    	sem_t s_;
    	bool ok_;
};
