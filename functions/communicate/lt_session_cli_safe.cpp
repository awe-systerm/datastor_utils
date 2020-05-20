#include <lt_function_error.h>
#include <lt_function/thread_pool.hpp>
#include "lt_session_cli_safe.h"
#include "lt_session_cli_set.h"
#include "../log/include/awe_log.h"

static data_channel::thread_pool discon_pool(CLI_DISCONNECT_WAIT_POOL_NUM);

void lt_session_cli_safe::connect(const std::string &ip, unsigned short port)
{
    try
    {
        std::unique_lock<std::mutex> lck(conn_m);
        if ( ++out_connect_ref == 1 )
        {
            connect_first(ip, port);
        }
        else if ( !is_down_connected )
        {
            reconnect(ip, port);
        }
    }
    catch (int &err)
    {
        --out_connect_ref;
        throw err;
    }
}

void lt_session_cli_safe::disconnect()
{
    if ( out_connect_ref.fetch_sub(1) == 1 )
    {
        disconnect_last();
    }
}

lt_session_cli_safe::lt_session_cli_safe(lt_session_cli_set *_set,
                                         boost::asio::io_service *_io_service,
                                         lt_session_callback *_cb) :
        lt_session_cli(_io_service, (lt_session_callback *) this),
        is_down_connected(false), cb(_cb), set(_set), pending_cnt(0)
{
    out_connect_ref.store(0);
}

void lt_session_cli_safe::disconnected_inthread()
{
    assert_legal();
    
    AWE_MODULE_DEBUG("communicate", "disconnected_inthread in %p", this);
    long pending = pending_cnt;
    if ( pending != 0 )
    {
        usleep(1000);
        if ( ++wait_disconn_num > MAX_DICONNECT_WAIT_NUM )
        {
            AWE_MODULE_WARN("communicate", "disconnected_inthread wait too long %p", this);
            wait_disconn_num= 0;
        }
        
        discon_pool.submit_task(
                    boost::bind(&lt_session_cli_safe::disconnected_inthread,
                                this));
        
    }
    else
    {
        AWE_MODULE_INFO("communicate", "disconnected_inthread before notice %p",
                        this);
        lt_session::disconnected();
        AWE_MODULE_INFO("communicate", "disconnected_inthread out");
    }
}


void lt_session_cli_safe::disconnected()
{
    std::unique_lock<std::mutex> lck(conn_m);
    is_down_connected = false;
    AWE_MODULE_INFO("communicate", "disconnected_inthread post %p", this);
    discon_pool.submit_task(
            boost::bind(&lt_session_cli_safe::disconnected_inthread, this));
}

void
lt_session_cli_safe::connect_first(const std::string &ip, unsigned short port)
{
    lt_session_cli::connect(ip, port);
    is_down_connected = true;
}

void lt_session_cli_safe::reconnect(const std::string &ip, unsigned short port)
{
    lt_session_cli::connect(ip, port);
    is_down_connected = true;
}

void lt_session_cli_safe::disconnect_last()
{
    lt_session_cli::disconnect();
}

void lt_session_cli_safe::rcv_done(lt_session *sess, lt_data_t *received_data,
                                   int error)
{
    assert_legal();
    cb->rcv_done(sess, received_data, error);
    __sync_sub_and_fetch(&pending_cnt, 1);
    set->put_session_internal(this);
}

void
lt_session_cli_safe::snd_done(lt_session *sess, lt_data_t *sent_data, int error)
{
    assert_legal();
    cb->snd_done(sess, sent_data, error);
    __sync_sub_and_fetch(&pending_cnt, 1);
    set->put_session_internal(this);
}

void lt_session_cli_safe::disconnected(lt_session *sess)
{
    cb->disconnected(sess);
}

void lt_session_cli_safe::connected(lt_session *sess)
{
    cb->connected(sess);
}

void lt_session_cli_safe::rcv(lt_data_t *data)
{
    AWE_MODULE_DEBUG("communicate",
                     "before rcv lt_session_cli_safe::snd sess %p", this);
    if ( !is_down_connected )
    {
        cb->rcv_done(this, data, -RPC_ERROR_TYPE_NET_BROKEN);
        return;
    }
    
    set->get_session_internal(this);
    __sync_add_and_fetch(&pending_cnt, 1);
    lt_session::rcv(data);
    AWE_MODULE_DEBUG("communicate",
                     "after rcv lt_session_cli_safe::snd sess %p", this);
}

void lt_session_cli_safe::snd(lt_data_t *data)
{
    AWE_MODULE_DEBUG("communicate",
                     "before snd lt_session_cli_safe::snd sess %p", this);
    if ( !is_down_connected )
    {
        cb->snd_done(this, data, -RPC_ERROR_TYPE_NET_BROKEN);
        return;
    }
    
    set->get_session_internal(this);
    __sync_add_and_fetch(&pending_cnt, 1);
    lt_session::snd(data);
    AWE_MODULE_DEBUG("communicate",
                     "after snd lt_session_cli_safe::snd sess %p", this);
}
