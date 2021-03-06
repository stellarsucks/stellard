//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "../../beast/beast/cxx14/memory.h" // <memory>

namespace ripple {

class IoServicePool::ServiceThread : private beast::Thread
{
public:
    explicit ServiceThread (
        beast::String const& name,
        IoServicePool& owner,
        boost::asio::io_service& service)
        : Thread (name)
        , m_owner (owner)
        , m_service (service)
    {
        startThread ();
    }

    ~ServiceThread ()
    {
        // block until thread exits
        stopThread ();
    }

    void start ()
    {
        startThread ();
    }

    void run ()
    {
        m_service.run ();

        m_owner.onThreadExit();
    }

private:
    IoServicePool& m_owner;
    boost::asio::io_service& m_service;
};

//------------------------------------------------------------------------------

IoServicePool::IoServicePool (Stoppable& parent, beast::String const& name,
                              int numberOfThreads)
    : Stoppable (name.toStdString().c_str(), parent)
    , m_name (name)
    , m_service (numberOfThreads)
    , m_work (boost::ref (m_service))
    , m_threadsDesired (numberOfThreads)
{
    bassert (m_threadsDesired > 0);
}

IoServicePool::~IoServicePool ()
{
    // the dtor of m_threads will block until each thread exits.
}

boost::asio::io_service& IoServicePool::getService ()
{
    return m_service;
}

IoServicePool::operator boost::asio::io_service& ()
{
    return m_service;
}

void IoServicePool::onStart ()
{
    m_threads.reserve (m_threadsDesired);
    for (int i = 0; i < m_threadsDesired; ++i)
    {
        m_threads.emplace_back (std::move (std::make_unique <
            ServiceThread> (m_name, *this, m_service)));
        ++m_threadsRunning;
        m_threads[i]->start ();
    }
}

void IoServicePool::onStop ()
{
    // VFALCO NOTE This is how it SHOULD work
    //
    //m_work = boost::none;
}

void IoServicePool::onChildrenStopped ()
{
    // VFALCO NOTE This is a hack! We should gracefully
    //             cancel all pending I/O, and delete the work
    //             object using boost::optional, and let run()
    //             just return naturally.
    //
    m_service.stop ();
}

// Called every time io_service::run() returns and a thread will exit.
//
void IoServicePool::onThreadExit()
{
    // service must be stopping for threads to exit.
    bassert (isStopping());

    // must have at least count 1
    bassert (m_threadsRunning.get() > 0);

    if (--m_threadsRunning == 0)
    {
        // last thread just exited
        stopped ();
    }
}

} // ripple
