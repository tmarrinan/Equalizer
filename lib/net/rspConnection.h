
/* Copyright (c) 2009, Cedric Stalder <cedric.stalder@gmail.com> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EQNET_RSPCONNECTION_H
#define EQNET_RSPCONNECTION_H

#include <eq/base/base.h>
#include <eq/net/types.h>
#include <eq/base/buffer.h> // member
#include <eq/base/monitor.h>
#include <eq/net/connectionType.h>
#include <eq/base/rng.h>
#include "connectionSet.h"
#include "udpConnection.h"
#include "fdConnection.h"
#ifndef WIN32
#  include <poll.h>
#  include "pipeConnection.h"
#endif
namespace eq
{
namespace net
{

    class ConnectionDescription;
    class RSPConnection;
    /** A rsp connection (Attn: only multicast usage implemented). */
    class RSPConnection
#ifdef WIN32
        : public Connection
#else
        : public FDConnection
#endif
    {

    public:
        /** Create a new rsp-based connection. */
        RSPConnection();
        virtual ~RSPConnection();

        virtual bool listen();
        enum DatagramType 
        { 
            // exchange datagram during data send
            DATA,      // the datagram contains data
            ACKREQ,    // ask for ack from all readers
            NACK,      // negative ack, request missing packets
            ACK,       // positive ack all data
            ID_HELLO, // a new node is connected.
            ID_DENY,
            ID_CONFIRM,
            ID_EXIT,  // a node is disconnected
            COUNTNODE  // send to other the number node which I have found
        };
        
        typedef uint16_t ID;
        typedef uint16_t IDSequenceType;
        struct DatagramAckRequest
        {
            uint8_t    type;
            ID   writerID;
            uint16_t           lastDatagramID;
            IDSequenceType     sequenceID;
        };
        
        struct DatagramNode
        {
            uint8_t            type;
            ID   connectionID;
        };

        struct DatagramCountConnection
        {
            uint8_t             type;
            ID    clientID;
            uint16_t             nbClient;
        };

        struct DatagramNack
        {
        
            uint8_t     type;
            ID    readerID;    // ID of the connection reader
            ID    writerID;    // ID of the connection writer
            IDSequenceType      sequenceID;  // last datagram in write sequence
            uint8_t             countRepeatID; // number of delta repeat
        };

        struct DatagramAck
        {
            uint8_t             type;
            ID    readerID;
            ID    writerID;
            IDSequenceType      sequenceID;
        };

        struct DatagramData
        {
            uint8_t     type;
            uint32_t    writeSeqID;
            uint32_t    dataIDlength;
        };
        
        struct DataReceive
        {
            DataReceive(){ reset(); }
            void reset();
            uint32_t    sequenceID;
            eq::base::Monitor< bool >  ackSend;
            eq::base::Monitor< bool >  allRead;
            uint64_t    posRead;
            base::Buffer< bool > boolBuffer;
            base::Bufferb dataBuffer;
        };

        struct WriteDatagramData
        {
            DatagramData    header;
            const void*     data;
        };

        
        void close();
        bool connect(){ return listen(); }

        virtual void acceptNB(){ EQASSERT( _state == STATE_LISTENING ); }

        virtual ConnectionPtr acceptSync();
        void readNB( void* buffer, const uint64_t bytes ){/* NOP */}
        int64_t readSync( void* buffer, const uint64_t bytes );
        int64_t write( const void* buffer, const uint64_t bytes );
        ConnectionPtr getSibling(){ return _sibling.get(); }

        
#ifdef WIN32
        /** @sa Connection::getNotifier */
        virtual Notifier getNotifier() const 
                   { return _hEvent; }
#endif
    
    private:

        typedef base::RefPtr< UDPConnection > UDPConnectionPtr;
        typedef base::RefPtr< RSPConnection > RSPConnectionPtr;
        
        /* managing RSP protocole directly on the udp connection */
        class Thread : public eq::base::Thread
        {
        public:
            Thread( RSPConnectionPtr connection )
                : _connection( connection ){}
            virtual ~Thread(){ _connection = 0; }
            virtual bool init(){ return _connection->_initReadThread(); }
        protected:
            
            virtual void* run();
        private:
            RSPConnectionPtr _connection;
        };

        ID _buildNewID();
        
        int64_t _readSync( DataReceive* receive, 
                           void* buffer, 
                           const uint64_t bytes  );
        

        /* using directly by the thread to manage RSP */
        bool _handleData( );
        bool _handleDataDatagram( const DatagramData* datagram );
        bool _handleAck( const DatagramAck* ack );
        bool _handleNack( const DatagramNack* nack );
        bool _handleAckRequest( const DatagramAckRequest* ackRequest );

        /** Initialize the reader thread */
        bool _initReadThread();

        /* Run the reader thread */
        void _runReadThread();

        bool _handleInitData();
        
        void _initAIOAccept(){ _initAIORead(); }
        void _exitAIOAccept(){ _exitAIORead(); }
        void _initAIORead();
        void _exitAIORead();

        // for read one datagram from udp Connection
        eq::base::Bufferb _bufRead;

        // a link for all connection in the multicast network 
        std::vector< RSPConnectionPtr > _children;
        // The buffer used by the read function in udp socket 
        std::vector< DataReceive* >_buffer;
        // number connection accepted by server RSP 
        uint8_t _countAcceptChildren;
        
        // The buffer used by the write function in udp socket
        eq::base::Bufferb _sendBuffer;

        //int _indexRead;

        eq::base::RNG         _rng;
        ID _id; //!< The identifier used to demultiplex multipe writers
        uint32_t _shiftedID; //!< Shifted _id for easier packaging

        ID _writerID;
        
        uint8_t  _countTimeOut;
        bool     _ackReceive;

#ifdef WIN32
        HANDLE _hEvent;
#else
        pollfd _hEvent;
        PipeConnection* _selfPipeHEvent;

        /** The buffer to receive commands from Event. */
        uint8_t _selfCommand;
#endif
        ConnectionSet _connectionSet;
        eq::base::Monitor< bool > _writing;
        uint32_t         _countNbAckInWrite;
        Thread*          _thread;
        UDPConnectionPtr _connection;
        base::Lock       _mutexConnection;
        base::Lock       _mutexEvent;
        RSPConnectionPtr _parent;
        
        // buffer for read from udp Connection
        DataReceive* _recvBuffer;
        int64_t     _lastSequenceIDAck;

        // write property part
        const char* _dataSend;
        uint64_t    _lengthDataSend;
        uint32_t    _numberDatagram;
        IDSequenceType    _sequenceIDWrite;

        // we add 1 each read or write operation
        uint8_t _readBufferIndex;
        uint8_t _recvBufferIndex;

        ConnectionPtr _sibling;
        
        DataReceive* _findReceiverRead();
        
        DataReceive* _findReceiverWithSequenceID( 
                                    const IDSequenceType sequenceID ) const;
        RSPConnectionPtr _findConnectionWithWriterID( 
                                    const ID writerID );
        bool _isCurrentSequenceWrite( const IDSequenceType sequenceID, 
                                      const ID writer );
        void _sendDatagramCountNode();
        void _sendDatagram( const uint32_t writSeqID,
                            const IDSequenceType datagramID );

        void _sendAckRequest( );
        void _sendAck( const ID writerID,
                       const IDSequenceType sequenceID);

        void _sendNackDatagram ( const ID  toWriterID,
                                 const IDSequenceType  sequenceID,
                                 const uint8_t   countNack,
                                 const uint32_t* repeatID   );
        
        bool _acceptNewIDConnection( const ID id );

        /* add a new connection detected in the network multicast */
        bool _addNewConnection( const ID id );

        bool _acceptRemoveConnection( const ID id );
        CHECK_THREAD_DECLARE( _recvThread );
 
    };
}
}

#endif //EQNET_RSPCONNECTION_H
