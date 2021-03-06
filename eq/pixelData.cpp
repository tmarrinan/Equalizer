
/* Copyright (c) 2006-2017, Stefan Eilemann <eile@equalizergraphics.com>
 *                          Daniel Nachbaur <danielnachbaur@gmail.com>
 *                          Cedric Stalder <cedric.stalder@gmail.com>
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

#include "pixelData.h"

#include <pression/plugins/compressor.h>

namespace eq
{
PixelData::PixelData()
{
    reset();
}

PixelData::PixelData(const PixelData& rhs)
    : internalFormat(rhs.internalFormat)
    , externalFormat(rhs.externalFormat)
    , pixelSize(rhs.pixelSize)
    , pvp(rhs.pvp)
    , compressedData(rhs.compressedData)
    , compressorName(rhs.compressorName)
    , compressorFlags(rhs.compressorFlags)
{
}

PixelData::~PixelData()
{
    reset();
}

void PixelData::reset()
{
    internalFormat = EQ_COMPRESSOR_DATATYPE_NONE;
    externalFormat = EQ_COMPRESSOR_DATATYPE_NONE;
    pixelSize = 0;
    pixels = 0;
    compressedData = pression::CompressorResult();
    compressorName = EQ_COMPRESSOR_INVALID;
    compressorFlags = 0;
}
}
