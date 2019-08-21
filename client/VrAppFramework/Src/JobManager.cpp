/************************************************************************************

Filename    :   JobManager.h
Content     :   A multi-threaded job manager
Created     :   12/15/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "JobManager.h"

#include <mutex>
#include <ctime>
#include <atomic>
#include <condition_variable>

#include "JniUtils.h"
#include "OVR_LogUtils.h"

namespace OVR {

//==============================================================================================
// ovrMPMCArray
//==============================================================================================

//==============================================================
// ovrMPMCArray
//
// Multiple-producer, multiple-consumer array.
// Currently implemented with a mutex.

template< typename T >
class ovrMPMCArray
{
public:
	ovrMPMCArray() { }
	~ovrMPMCArray() { }

	T 			Pop();
	void		PushBack( T const & value );
	void		RemoveAt( size_t const index );
	void		RemoveAtUnordered( size_t const index );
	T const 	operator[] ( int const index ) const;
	T 			operator[] ( int const index );
	void		Clear();
	void		MoveArray( std::vector< T > & a );

private:
	std::vector< T >	A;
	std::mutex			ThisMutex;
};

template< typename T >
T ovrMPMCArray< T >::Pop()
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	if ( A.size() <= 0 )
	{
		return nullptr;
	}
	T item = A.back();
	A.pop_back();
	return item;
}

template< typename T >
void ovrMPMCArray< T >::PushBack( T const & value )
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	A.push_back( value );
}

template< typename T >
void ovrMPMCArray< T >::RemoveAt( size_t const index )
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	return A.erase( A.cbegin() + index );
}

template< typename T >
void ovrMPMCArray< T >::RemoveAtUnordered( size_t const index )
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	return A.erase( A.cbegin() + index );
}
	
template< typename T >
T const ovrMPMCArray< T >::operator[] ( int const index ) const
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	return A[index];
}

template< typename T >
T ovrMPMCArray< T >::operator[] ( int const index )
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	return A[index];
}

template< typename T >
void ovrMPMCArray< T >::Clear()
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	A.clear();
}

template< typename T >
void ovrMPMCArray< T >::MoveArray( std::vector< T > & a )
{
	std::lock_guard< std::mutex > mutex( ThisMutex );
	a.assign( A.cbegin(), A.cend() );
	A.clear();
}


class ovrJobManagerImpl;

//==============================================================
// ovrJobThread
class ovrJobThread
{
public:
	ovrJobThread( ovrJobManagerImpl * jobManager, char const * threadName )
		: JobManager( jobManager )
		, Jni( nullptr )
		, Attached( false )
	{
		OVR_strcpy( ThreadName, sizeof( ThreadName ), threadName );
	}
	~ovrJobThread()
	{
		// verify shutown before deconstruction
		OVR_ASSERT( Jni == nullptr );
	}

	static ovrJobThread *	Create( ovrJobManagerImpl * jobManager, const char * threadName );
	static void				Destroy( ovrJobThread * & jobThread );

	void	ThreadFunction();

	void	Init();
	void	Shutdown();

	ovrJobManagerImpl *	GetJobManager() { return JobManager; }
	JNIEnv *			GetJni() { return Jni; }
	char const *		GetThreadName() const { return ThreadName; }
	bool				IsAttached() const { return Attached; }

private:
	ovrJobManagerImpl *	JobManager;	// manager that owns us
	std::thread			MyThread;	// our thread context
	JNIEnv *			Jni;		// Java environment for this thread
	char				ThreadName[16];
	bool				Attached;

private:
	void	AttachToCurrentThread();
	void	DetachFromCurrentThread();
};

//==============================================================
// ovrJobManagerImpl
class ovrJobManagerImpl : public ovrJobManager
{
public:
	friend class ovrJobThread;

	static const int	MAX_THREADS = 5;

	ovrJobManagerImpl();
	virtual ~ovrJobManagerImpl();

	void	Init( JavaVM & javaVM ) OVR_OVERRIDE;
	void	Shutdown() OVR_OVERRIDE;

	void	EnqueueJob( ovrJob * job ) OVR_OVERRIDE;

	void	ServiceJobs( std::vector< ovrJobResult > & finishedJobs ) OVR_OVERRIDE;

	bool	IsExiting() const OVR_OVERRIDE { return Exiting; }

	JavaVM *GetJvm() { return Jvm; }

private:
	//--------------------------
	// thread function interface
	//--------------------------
	void		JobCompleted( ovrJob * job, bool const succeeded );
	ovrJob *	GetPendingJob();

	void		WaitForJobs() { std::unique_lock< std::mutex > lk( JobMutex ); JobCV.wait( lk ); }

private:
	std::vector< ovrJobThread* >	Threads;

	ovrMPMCArray< ovrJob* >			PendingJobs;	// jobs that haven't executed yet
	ovrMPMCArray< ovrJob* >			RunningJobs;	// jobs that are currently executing

	ovrMPMCArray< ovrJobResult >	CompletedJobs;	// jobs that have completed

	std::mutex 						JobMutex;
	std::condition_variable 		JobCV;

	bool							Initialized;
	volatile bool					Exiting;

	JavaVM *						Jvm;

private:
//	bool	StartJob( OVR::Thread * thread, ovrJob * job );
//	bool	StartJob( ovrJob * job );
	void	AttachToCurrentThread();
	void	DetachFromCurrentThread();
};

//==============================================================================================
// ovrJob
//==============================================================================================
ovrJob::ovrJob( char const * name )
{
	OVR_strcpy( Name, sizeof( Name ), name );
}

void ovrJob::DoWork( ovrJobThreadContext const & jtc )
{
	clock_t startTime = std::clock();

	DoWork_Impl( jtc );

	clock_t endTime = std::clock();
	OVR_LOG( "Job '%s' took %f seconds.", Name, double( endTime - startTime ) / (double)CLOCKS_PER_SEC );
}

//==============================================================================================
// ovrJobThread
//==============================================================================================

void ovrJobThread::ThreadFunction()
{
	ovrJobManagerImpl * jm = GetJobManager();

	// thread->SetThreadName( GetThreadName() );

	AttachToCurrentThread();

	while ( !jm->IsExiting() )
	{
		ovrJob * job = jm->GetPendingJob();
		if ( job != nullptr )
		{
			ovrJobThreadContext context( jm->GetJvm(), GetJni() );
			job->DoWork( context );
			jm->JobCompleted( job, true );
		}
		else
		{
			jm->WaitForJobs();
		}
	}

	DetachFromCurrentThread();
}

ovrJobThread * ovrJobThread::Create( ovrJobManagerImpl * jobManager, char const * threadName )
{
	ovrJobThread * jobThread = new ovrJobThread( jobManager, threadName );
	if ( jobThread == nullptr )
	{
		return nullptr;
	}
	
	jobThread->Init();
	return jobThread;
}

void ovrJobThread::Destroy( ovrJobThread * & jobThread )
{
	OVR_ASSERT( jobThread != nullptr );
	jobThread->Shutdown();
	delete jobThread;
	jobThread = nullptr;
}

void ovrJobThread::Init()
{
	OVR_ASSERT( JobManager != nullptr );
	MyThread = std::thread( &ovrJobThread::ThreadFunction, this );
}

void ovrJobThread::Shutdown()
{
	OVR_ASSERT( Jni == nullptr );	// DetachFromCurrentThread should have been called first
	OVR_ASSERT( MyThread.joinable() );

	if ( MyThread.joinable() )
	{
		MyThread.join();
	}
}

void ovrJobThread::AttachToCurrentThread()
{
	ovr_AttachCurrentThread( JobManager->GetJvm(), &Jni, nullptr );
	Attached = true;
}

void ovrJobThread::DetachFromCurrentThread()
{
	ovr_DetachCurrentThread( JobManager->GetJvm() );
	Jni = nullptr;
	Attached = false;
}

//==============================================================================================
// ovrJobManagerImpl
//==============================================================================================
ovrJob * ovrJobManagerImpl::GetPendingJob()
{
	ovrJob * pendingJob = PendingJobs.Pop();
	return pendingJob;
}

void ovrJobManagerImpl::JobCompleted( ovrJob * job, bool const succeeded )
{
	CompletedJobs.PushBack( ovrJobResult( job, succeeded ) );
}

ovrJobManagerImpl::ovrJobManagerImpl()
	: Initialized( false )
	, Exiting( false )
	, Jvm( nullptr )
{
}

ovrJobManagerImpl::~ovrJobManagerImpl()
{
	Shutdown();
}

void ovrJobManagerImpl::Init( JavaVM & javaVM )
{
	Jvm = &javaVM;

	// create all threads... they will end up waiting on a new job signal
	for ( int i = 0; i < MAX_THREADS; ++i )
	{
		char threadName[16];
		OVR_sprintf( threadName, sizeof( threadName ), "ovrJobThread_%i", i );

		ovrJobThread * jt = ovrJobThread::Create( this, threadName );
		Threads.push_back( jt );
	}

	Initialized = true;
}

void ovrJobManagerImpl::Shutdown()
{
	OVR_LOG( "ovrJobManagerImpl::Shutdown" );

	Exiting = true;

	// allow all threads to complete their current job
	// waiting threads must timeout waiting for JobCV
	while( Threads.size() > 0 )
	{
		// raise signal to release any waiting thread
		JobCV.notify_all();

		// loop through threads until we find one that's seen the Exiting flag and detatched
		int threadIndex = 0;
		for ( ; ; )
		{
			if ( !Threads[threadIndex]->IsAttached() )
			{
				OVR_LOG( "Exited thread '%s'", Threads[threadIndex]->GetThreadName() );
				ovrJobThread::Destroy( Threads[threadIndex] );
				Threads[ threadIndex ] = Threads.back();
				Threads.pop_back();
				break;
			}
			threadIndex++;
			if ( threadIndex >= static_cast< int >( Threads.size() ) )
			{
				threadIndex = 0;
			}
		}
	}

	Initialized = false;

	OVR_LOG( "ovrJobManagerImpl::Shutdown - complete." );
}

void ovrJobManagerImpl::EnqueueJob( ovrJob * job )
{
	//OVR_LOG( "ovrJobManagerImpl::EnqueueJob" );
	PendingJobs.PushBack( job );

	// signal a waiting job
	JobCV.notify_all();
}

void ovrJobManagerImpl::ServiceJobs( std::vector< ovrJobResult > & completedJobs )
{
	CompletedJobs.MoveArray( completedJobs );
}

ovrJobManager *	ovrJobManager::Create( JavaVM & javaVm )
{
	ovrJobManager * jm = new ovrJobManagerImpl();
	if ( jm != nullptr )
	{
		jm->Init( javaVm );
	}
	return jm;
}

void ovrJobManager::Destroy( ovrJobManager * & jm )
{
	if ( jm != nullptr )
	{
		delete jm;
		jm = nullptr;
	}
}

} // namespace OVR
