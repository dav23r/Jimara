#pragma once
#include "../../Core/Object.h"
#include <string>
#include <mutex>

namespace Jimara {
	namespace OS {
		/// <summary>
		/// Generic logger interface for logging messages, errors and what not
		/// </summary>
		class Logger : public virtual Object {
		public:
			enum class LogLevel : uint8_t {
				/// <summary> Logs like info, but only in debug mode </summary>
				LOG_DEBUG = 0,

				/// <summary> Informational log for notifying about progress and what not </summary>
				LOG_INFO = 1,

				/// <summary> Notification, that something might not be quite right, but the program will still work fine </summary>
				LOG_WARNING = 2,

				/// <summary> Some error occured, but the application will not crash </summary>
				LOG_ERROR = 3,

				/// <summary> Fatal error; application will exit </summary>
				LOG_FATAL = 4,

				/// <summary> Not an actual log level... This just tells how many log levels are available </summary>
				LOG_LEVEL_COUNT = 5
			};

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="minLogLevel"> Minimal log level that should not be ignored </param>
			inline Logger(LogLevel minLogLevel = LogLevel::LOG_DEBUG) : m_minLogLevel(minLogLevel) { }

			/// <summary> Virtual destructor </summary>
			inline virtual ~Logger() {}

			/// <summary> Minimal log level that should not be ignored </summary>
			inline LogLevel MinLogLevel()const { return m_minLogLevel; }

			/// <summary> Minimal log level that should not be ignored </summary>
			inline LogLevel& MinLogLevel() { return m_minLogLevel; }

			/// <summary>
			/// Generic log call
			/// </summary>
			/// <typeparam name="MessageType"> const char* or string </typeparam>
			/// <param name="level"> Log level </param>
			/// <param name="message"> Log message </param>
			/// <param name="exitCode"> Exit code (ignored, if level is not FATAL) </param>
			template<typename MessageType>
			inline void Log(LogLevel level, const char* message, int exitCode = 1) { BasicLog(level, message, exitCode); }

			/// <summary>
			/// equivalent of Log(LogLevel::LOG_DEBUG, message)
			/// </summary>
			/// <typeparam name="MessageType"> const char* or string </typeparam>
			/// <param name="message"> Log message </param>
			template<typename MessageType>
			inline void Debug(const MessageType& message) {
#ifndef NDEBUG
				BasicLog(LogLevel::LOG_DEBUG, message, 1);
#else
				(void)message;
#endif
			}

			/// <summary>
			/// equivalent of Log(LogLevel::LOG_INFO, message)
			/// </summary>
			/// <typeparam name="MessageType"> const char* or string </typeparam>
			/// <param name="message"> Log message </param>
			template<typename MessageType>
			inline void Info(const MessageType& message) { BasicLog(LogLevel::LOG_INFO, message, 1); }

			/// <summary>
			/// equivalent of Log(LogLevel::LOG_WARNING, message)
			/// </summary>
			/// <typeparam name="MessageType"> const char* or string </typeparam>
			/// <param name="message"> Log message </param>
			template<typename MessageType>
			inline void Warning(const MessageType& message) { BasicLog(LogLevel::LOG_WARNING, message, 1); }

			/// <summary>
			/// equivalent of Log(LogLevel::LOG_ERROR, message)
			/// </summary>
			/// <typeparam name="MessageType"> const char* or string </typeparam>
			/// <param name="message"> Log message </param>
			template<typename MessageType>
			inline void Error(const MessageType& message) { BasicLog(LogLevel::LOG_ERROR, message, 1); }

			/// <summary>
			/// equivalent of Log(LogLevel::LOG_FATAL, message)
			/// </summary>
			/// <typeparam name="MessageType"> const char* or string </typeparam>
			/// <param name="message"> Log message </param>
			/// <param name="exitCode"> Exit code </param>
			template<typename MessageType>
			inline void Fatal(const MessageType& message, int exitCode = 1) { BasicLog(LogLevel::LOG_FATAL, message, exitCode); }



			/// <summary> Log call information </summary>
			struct LogInfo {
				/// <summary> Log level </summary>
				LogLevel level;

				/// <summary> Log message (memory managed externally) </summary>
				const char* message;

				/// <summary> 
				/// Optional exit code information for FATAL calls 
				/// (keep in mind, that for fatal cases this code will be returned automagically, 
				/// LogInfo only contains it only for informational purposes) 
				/// </summary>
				int exitCode;
			};



		protected:
			/// <summary>
			/// Should record/display the log (no synchronisation needed; it's already thread-safe by design)
			/// </summary>
			/// <param name="info"> Log information </param>
			virtual void Log(const LogInfo& info) = 0;



		private:
			// Lock
			std::recursive_mutex m_logLock;

			// Minimal log level that will not be ignored
			LogLevel m_minLogLevel;

			// Underlying log logic
			void BasicLog(LogLevel level, const char* message, int exitCode, size_t stackOffset = 0);

			// Underlying log logic (for std::string)
			inline void BasicLog(LogLevel level, const std::string& message, int exitCode) { BasicLog(level, message.c_str(), exitCode, 1); }
		};
	}
}
