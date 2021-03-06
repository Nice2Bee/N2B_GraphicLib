/*
NB_Error_Log:
Attention:
	!Never use this Error_Log in an Application that must
	run a long time without disabling the log vector!
	It will be proved if the filenames of the logs are correct
	but not if the files are openable.
Purpose:
	The NB_Error_Log should take less performance but give
	Real-time feedback.
	To achieve this, all errors that are send to the error log
	are pushed into a queue. A thread is started in the constructor
	that takes by default one error per second out of this queue
	and pushes it into a log file.
	It will also save all errors into a vector, this could be a
	memory problem if too many errors occur.
Usage:
	The Error_Log can be initialized with an Error class, or by default
	with the NB_Error.
*/

#define NB_PRAGMA_ONCE_SUPPORT
#ifdef NB_PRAGMA_ONCE_SUPPORT
#pragma once
#endif
#ifndef NB_ERROR_LOG_HPP_INCLUDED
#define NB_ERROR_LOG_HPP_INCLUDED
#include <vector>
#include <ctime>
#include <mutex>
#include <queue>
#include <fstream>
#include <regex>
#include <atomic>

namespace NB
{
	//CLASS NB_Error	
	/*
	Default Error class that defines the needed outstream operator and the needed int id.
	It sets a time stamp in the constructor, but is very simple.
	*/
	enum NB_Error_Flag
	{
		NB_FATAL_ERROR = 1,
		NB_WARNING = 1 << 1,
		NB_ERROR = 1 << 2
	};

	class NB_Error
	{
	public:
		NB_Error(NB_Error_Flag signature, const std::string location, const std::string error);
		NB_Error_Flag signature;
		std::string location;
		std::string error_name;
		std::time_t time_stamp;
		int id;

		friend std::ostream& operator<<(std::ostream& os, const NB_Error& err);
	};
	std::ostream& operator<<(std::ostream& os, const NB_Error& err);


	//CLASS ERROR_LOG
	template <class Error_T>
	class NB_Error_Log;

	/*
	The Thread_Queue is very simple now.
	It would be possible to implement a virtual table free, lock free version.
	But no need at the moment.
	*/
	template <class Error_T>
	class Thread_Queue : private std::queue<Error_T*>
	{
		friend class NB_Error_Log<Error_T>;
	private:
		std::mutex mutex;
	};


	/**NB_Error_Log
	Attention:
		Never use this Error_Log in an Application that has to
		run a long time without disabling the log vector!
	Purpose:
		The NB_Error_Log should take less performance but give
		Real-time feedback.
		To achieve this, all errors that are send to the error log
		are pushed into a queue. A thread is started in the constructor
		that takes by default one error per second out of this queue
		and pushes it into a log file.
		It will also save all errors into a vector, this could be a
		memory problem if too many errors occur.
	Usage:
		The Error_Log can be initialized with an Error class, or by default
		with the NB_Error.

	Discription:

	Initialization:
		The class NB_Error_Log needs to be initialized with an Error class
		The class Error must implement an:
		int id
		and the ostream operator:
		ostream& operator<<(ostream&, const Error&)
		@see NB::NB_Error

	Log Files:
		After initialization it will append the logged errors to a file
		error.log (this file will never be deleted)

		and the final error log of the session to
		final_error.log (this file will be overwritten every session)

		this names can be changed by calling
		@see NB::NB_Error_Log::set_log_name()
		and
		@see NB::NB_Error_Log::set_final_log_name()
		but both of this functions throw a
		std::runtime_error if the file name is invalid.

	Log:
		To log an error the new error must be created and pass over to
		the err_log(Error*) function. NB_Error_Log will now take care of
		the memory.
		Example: my_log.err_log(new My_Error());
		@see NB::NB_Error_Log::err_log()

	Working method:
		In standard mode one thread will handle the errors that are
		added to the work queue by calling the function log(Error_T*).
		It will prove every second if there is something new in the
		work queue and push only one of the errors into the log file.
		@see NB::NB_Error_Log::handle_work()

		By calling the function set_log_rate(std::chrono::microseconds)
		the rate can be changed. But it is also possible to print all
		errors into the file by just calling get_queue_ready().
		@see NB::NB_Error_Log::set_log_rate()
		@see NB::NB_Error_Log::get_queue_ready()

	Get Errors:
		The function std::stringstream print_errors(bool emty_queue = false)
		can be called to get a stringstream of all errors. If there are still
		errors in the queue it will also print the number of not listed errors.
		If it is called with the argument true it will first call get_queue_ready()
		to ensure that there are no more Errors in the work_queue.
		@see NB::NB_Error_Log::print_errors()
	*/
	template <class Error_T = NB_Error>
	class NB_Error_Log
	{
	public:
		NB_Error_Log();
		~NB_Error_Log();


		void set_log_name(const std::string log_file_name);
		void set_final_log_name(const std::string final_log_file_name);
		void set_log_rate(const std::chrono::microseconds& rate);

		void err_log(Error_T&& err);

		std::stringstream print_errors(bool emty_queue = false);
		void get_queue_ready();
	private:
		[[noreturn]] void handle_work();
		void save_error(const Error_T& err, std::ofstream& file);
		void open_file_append(const std::string& file_name, std::ofstream& file);

		bool is_name_valid(const std::string file_name);

		std::string log_file_name;
		std::string final_log_file_name;

		int error_id;
		std::vector<Error_T*> log_vec;
		Thread_Queue<Error_T> work_q;
		std::thread work_handler;
		std::atomic<bool> is_running;
		std::chrono::microseconds log_rate;
	};
}


/*
When NB_Error_Log is initialized it will start his work handler
and new logs can be added with the function log()
*/
template <class Error_T>
NB::NB_Error_Log<Error_T>::NB_Error_Log()
	: is_running(true),
	log_file_name("error.log"),
	final_log_file_name("final_error.log"),
	error_id(0),
	log_rate(std::chrono::microseconds(1000000))
{
	work_handler = std::thread(&NB::NB_Error_Log<Error_T>::handle_work, this);
}

//throws runtime_error if name is not valid
template <class Error_T>
void NB::NB_Error_Log<Error_T>::set_log_name(const std::string log_file_name)
{
	if (is_name_valid(log_file_name))
		this->log_file_name = log_file_name;
}

//throws runtime_error if name is not valid
template <class Error_T>
void NB::NB_Error_Log<Error_T>::set_final_log_name(const std::string final_log_file_name)
{
	if (is_name_valid(final_log_file_name))
		this->final_log_file_name = final_log_file_name;
}


/*
if the given name is not valid there is no possible comeback without the risk
of overwriting important data so, the throw is considered the best alternative
*/
template <class Error_T>
bool NB::NB_Error_Log<Error_T>::is_name_valid(const std::string file_name)
{
	std::regex invalid_chars("[\\/:*?\"<>|]");
	if (std::regex_search(file_name, invalid_chars)
		|| file_name.length() == 0)
		throw std::runtime_error("NB::NB_Error_Log::NB_Error_Log()\n"
			"\"" + file_name + "\" is not a valid filename\n");
	return true;
}

//sets the rate with which the work_queue will be emptied
template <class Error_T>
void NB::NB_Error_Log<Error_T>::set_log_rate(const std::chrono::microseconds& rate)
{
	this->log_rate = rate;
}

/*
When the log is destroyed, it will ensure that the thread work_handler
has consumed all waiting errors of the work_q.
After that it will try to save a final version of the error log. If it
fails it will put the information to the cerr console
*/
template <class Error_T>
NB::NB_Error_Log<Error_T>::~NB_Error_Log()
{
	//inform the thread work_handler that the program wants to close
	is_running = false;

	//wait for the work_handler to become joinable
	bool is_waiting = true;
	while (is_waiting)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (!work_handler.joinable())
			is_waiting = false;
	}

	//try to save to file
	//show on console if failed
	std::ofstream file;
	if (log_vec.size() > 0)
		file.open(final_log_file_name, std::ios::out);
	if (!file)
	{
		std::cerr << "\nNot able to save Error Log:\n";
		for (auto& i : log_vec)
			std::cerr << *i << std::endl;
	}
	else
	{
		for (auto& itr = std::rbegin(log_vec); itr < std::rend(log_vec); itr++)
		{
			file << **itr << std::endl;
		}
	}

	//clean up
	for (auto& i : log_vec)
		delete i;
}

/*
Prints all handles errors to console
*/
template <class Error_T>
std::stringstream NB::NB_Error_Log<Error_T>::print_errors(bool emty_queue)
{
	std::unique_lock<std::mutex> guard(work_q.mutex, std::defer_lock);
	if (emty_queue)
	{
		get_queue_ready();
	}

	stringstream ss;
	ss << "\nErrors:\n\n";
	for (auto& i : log_vec)
		ss << *i << std::endl;

	guard.lock();
	int wk_s = work_q.size();
	guard.unlock();

	if (wk_s > 0)
		ss << "\nThere are still " << wk_s << " Errors in the work_q\n"
		"Use argument \"true\" to empty\n";
	return ss;
}

/*
Saves an error to a given file.
Just a function to make it easier to add additional
Information to the saved Error if necessary
*/
template <class Error_T>
void NB::NB_Error_Log<Error_T>::save_error(const Error_T& err, std::ofstream& file)
{
	file << err << std::endl;
}

/*
Opens a file in append mode, prints error to console if failed
*/
template <class Error_T>
void NB::NB_Error_Log<Error_T>::open_file_append(const std::string& file_name, std::ofstream& file)
{
	file.open(file_name, std::ios::app);
	if (!file.is_open())
		std::cerr << "\nCould not open " + file_name << std::endl;
}

/*
Will push an error to the work_queue as fast as possible (+ 1 virtual function table lookup)
*/
template <class Error_T>
void NB::NB_Error_Log<Error_T>::err_log(Error_T&& err)
{
	std::lock_guard<std::mutex> guard(work_q.mutex);
	work_q.push(new Error_T(err));
}

/*
Is only used by the thread work_handler.
handel means it pushes the Errors into a vector and appends it to a file.

Will empty the work_queue with as less as possible time owning it.
To reduce the owning time further it handles only one Error per second.

If the program terminates it will handle the remaining
Errors as fast as possible.
*/
template <class Error_T>
[[noreturn]] void NB::NB_Error_Log<Error_T>::handle_work()
{
	std::unique_lock<std::mutex> guard(work_q.mutex, std::defer_lock);
	while (true)
	{
		//sleep for one second
		std::this_thread::sleep_for(log_rate);

		//lock to get size
		guard.lock();
		if (work_q.size() > 0)
		{
			//open file while unlocked
			guard.unlock();
			std::ofstream file;
			open_file_append(log_file_name, file);
			if (file.is_open())
			{
				//lock and get the Error*
				guard.lock();
				typename Error_T* err = work_q.front();
				guard.unlock();

				//set id
				err->id = error_id++;

				//push to the vector and save while unlocked
				log_vec.push_back(err);
				save_error(*err, file);

				//lock to pop front
				guard.lock();
				work_q.pop();
				guard.unlock();
			}
		}
		else
		{
			//unlock if size was 0 continue
			guard.unlock();
		}

		//if the Destructor was called clean up the queue
		if (!is_running)
		{
			get_queue_ready();

			work_handler.detach();
			work_handler.~thread();
		}
	}
}

/*
Will lock the queue.
Does the same as handle_work but faster and without
unlocking between all steps
*/
template <class Error_T>
void NB::NB_Error_Log<Error_T>::get_queue_ready()
{
	std::unique_lock<std::mutex> guard(work_q.mutex, std::defer_lock);
	guard.lock();
	std::ofstream file;
	if (work_q.size() > 0)
	{
		open_file_append(log_file_name, file);
	}
	while (work_q.size() > 0)
	{
		typename Error_T* err = work_q.front();

		err->id = error_id++;

		log_vec.push_back(err);
		save_error(*err, file);

		work_q.pop();
	}
	guard.unlock();
}

#endif // !NB_ERROR_LOG_HPP_INCLUDED

