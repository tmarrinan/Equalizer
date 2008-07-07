
/* Copyright (c) 2006-2008, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQ_FRAMEDATA_H
#define EQ_FRAMEDATA_H

#include <eq/client/frame.h>         // enum Frame::Buffer
#include <eq/client/pixelViewport.h> // member
#include <eq/client/pixel.h>         // member
#include <eq/client/range.h>         // member

#include <eq/base/monitor.h>         // member
#include <eq/net/object.h>           // base class

#include <set>                       // member
namespace eqs
{
    class FrameData;
}

namespace eq
{
    class  Image;
    struct FrameDataTransmitPacket;

    /**
     * A frame data holds multiple images and is used by frames.
     * It is not intended to be used directly by application code.
     */
    class EQ_EXPORT FrameData : public eqNet::Object
    {
    public:
        FrameData();
        virtual ~FrameData();

        /** 
         * @name Data Access
         */
        //*{
        /** The enabled frame buffer attachments. */
        uint32_t getBuffers() const { return _data.buffers; }
        void     setBuffers( const uint32_t buffers ){ _data.buffers = buffers;}

        /** The database-range relative to the destination channel. */
        const Range& getRange() const { return _data.range; }
        void setRange( const Range& range ) { _data.range = range; }
        
        /** The pixel decomposition relative to the destination channel. */
        const Pixel& getPixel() const { return _data.pixel; }
        
        /** The images of this frame data holder */
        const ImageVector& getImages() const { return _images; }

        /** The covered area. */
        void setPixelViewport( const PixelViewport& pvp ) { _data.pvp = pvp; }
        //*}

        /**
         * @name Operations
         */
        //*{

        /** Flush the frame by deleting all images. */
        void flush();

        /** 
         * Allocate and add a new image.
         * 
         * @return the image.
         */
        Image* newImage();

        /** Clear the frame by recycling the attached images. */
        void clear();

        /** 
         * Read back a set of images according to the current frame data.
         * 
         * The newly read images are added to the data, existing images are
         * retained.
         *
         * @param frame the corresponding output frame holder.
		 * @param glObjects the GL object manager for the current GL context.
         */
        void startReadback( const Frame& frame, 
                            Window::ObjectManager* glObjects );

        /** Synchronize the last image readback. */
        void syncReadback();

        /** 
         * Transmit the frame data to the specified node.
         *
         * Used internally after readback to push the image data to the input
         * frame nodes. Do not use directly.
         * 
         * @param toNode the receiving node.
         */        
        void transmit( eqNet::NodePtr toNode );

        /** @return true if the frame data is ready, false if not. */
        bool isReady() const   { return _readyVersion >= getVersion(); }

        /** Wait for the frame data to become available. */
        void waitReady() const { _readyVersion.waitGE( getVersion( )); }

        /** 
         * Add a listener which will be incremented when the frame is
         * ready.
         * 
         * @param listener the listener.
         */
        void addListener( eq::base::Monitor<uint32_t>& listener );

        /** 
         * Remove a frame listener.
         * 
         * @param listener the listener.
         */
        void removeListener( eq::base::Monitor<uint32_t>& listener );
        
        /** 
         * Disable the usage of a frame buffer attachment for all images.
         * 
         * @param buffer the buffer to disable.
         */
        void disableBuffer( const Frame::Buffer buffer )
            { _data.buffers &= ~buffer; }
        //*}

    protected:
        virtual ChangeType getChangeType() const { return INSTANCE; }

        /** @sa eqNet::Object::attachToSession */
        virtual void attachToSession( const uint32_t id, 
                                      const uint32_t instanceID, 
                                      eqNet::Session* session );
        /** @sa eqNet::Object::applyInstanceData */
        virtual void applyInstanceData( eqNet::DataIStream& is );

    private:
        struct Data
        {
            Data() : offset( vmml::Vector2i::ZERO ), buffers( 0 ), format( 0 )
                   , type( 0 ) {}

            PixelViewport  pvp;
            vmml::Vector2i offset;
            uint32_t       buffers;
            uint32_t       format;
            uint32_t       type;
            Range          range; //<! database-range of src wrt to dest channel
            Pixel          pixel; //<! pixel decomposition of source
        }
            _data;

        friend class eqs::FrameData;

        ImageVector _images;
        ImageVector _imageCache;
        eq::base::SpinLock    _imageCacheLock;

        struct ImageVersion
        {
            ImageVersion( Image* _image, const uint32_t _version )
                    : image( _image ), version( _version ) {}

            Image*   image;
            uint32_t version;
        };
        std::list<ImageVersion> _pendingImages;
        std::set< uint32_t >    _readyVersions;

        /** Data ready monitor synchronization primitive. */
        eq::base::Monitor<uint32_t> _readyVersion;

        /** External monitors for readyness synchronization. */
        std::vector< eq::base::Monitor<uint32_t>* > _listeners;
        eq::base::Lock                              _listenersMutex;

        /** Allocate or reuse an image. */
        Image* _allocImage();

        /** Apply all received images of the given version. */
        void _applyVersion( const uint32_t version );

        /** 
         * Set the frame data ready.
         * 
         * The frame data is automatically set ready by syncReadback
         * and upon processing of the transmit commands.
         */
        void _setReady( const uint32_t version );

        void _transmit( eqNet::NodePtr toNode,
                        FrameDataTransmitPacket& packet,
                        const Frame::Buffer buffers );

        /* The command handlers. */
        eqNet::CommandResult _cmdTransmit( eqNet::Command& command );
        eqNet::CommandResult _cmdReady( eqNet::Command& command );
        eqNet::CommandResult _cmdUpdate( eqNet::Command& command );

        CHECK_THREAD_DECLARE( _commandThread );
    };
    std::ostream& operator << ( std::ostream& os, const FrameData* data );
}

#endif // EQ_FRAMEDATA_H

