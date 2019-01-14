#include <dht/context.hpp>
#include <dht/messages/gotrouter.hpp>

#include <router/router.hpp>

namespace llarp
{
  namespace dht
  {
    GotRouterMessage::~GotRouterMessage()
    {
    }

    bool
    GotRouterMessage::BEncode(llarp_buffer_t *buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      // message type
      if(!BEncodeWriteDictMsgType(buf, "A", "S"))
        return false;

      if(K)
      {
        if(!BEncodeWriteDictEntry("K", *K.get(), buf))
          return false;
      }

      // near
      if(N.size())
      {
        if(!BEncodeWriteDictList("N", N, buf))
          return false;
      }

      if(!BEncodeWriteDictList("R", R, buf))
        return false;

      // txid
      if(!BEncodeWriteDictInt("T", txid, buf))
        return false;

      // version
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    GotRouterMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
    {
      if(llarp_buffer_eq(key, "K"))
      {
        if(K)  // duplicate key?
          return false;
        K.reset(new dht::Key_t());
        return K->BDecode(val);
      }
      if(llarp_buffer_eq(key, "N"))
      {
        return BEncodeReadList(N, val);
      }
      if(llarp_buffer_eq(key, "R"))
      {
        return BEncodeReadList(R, val);
      }
      if(llarp_buffer_eq(key, "T"))
      {
        return bencode_read_integer(val, &txid);
      }
      bool read = false;
      if(!BEncodeMaybeReadVersion("V", version, LLARP_PROTO_VERSION, read, key,
                                  val))
        return false;

      return read;
    }

    bool
    GotRouterMessage::HandleMessage(
        llarp_dht_context *ctx,
        __attribute__((unused))
        std::vector< std::unique_ptr< IMessage > > &replies) const
    {
      auto &dht = ctx->impl;
      if(relayed)
      {
        auto pathset = ctx->impl.router->paths.GetLocalPathSet(pathID);
        return pathset && pathset->HandleGotRouterMessage(this);
      }
      // not relayed
      TXOwner owner(From, txid);

      if(dht.pendingExploreLookups.HasPendingLookupFrom(owner))
      {
        if(N.size() == 0)
          dht.pendingExploreLookups.NotFound(owner, K);
        else
        {
          dht.pendingExploreLookups.Found(owner, From.as_array(), N);
        }
        return true;
      }
      // not explore lookup

      if(!dht.pendingRouterLookups.HasPendingLookupFrom(owner))
      {
        llarp::LogWarn("Unwarrented GRM from ", From, " txid=", txid);
        return false;
      }
      // no pending lookup

      llarp::LogInfo("DHT no pending lookup");
      if(R.size() == 1)
        dht.pendingRouterLookups.Found(owner, R[0].pubkey, {R[0]});
      else
        dht.pendingRouterLookups.NotFound(owner, K);
      return true;
    }
  }  // namespace dht
}  // namespace llarp
