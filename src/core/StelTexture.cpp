/*
 * Stellarium
 * Copyright (C) 2006 Fabien Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */
#include <cstdlib>
#include "StelTextureMgr.hpp"
#include "StelTexture.hpp"
#include "glues.h"
#include "StelFileMgr.hpp"
#include "StelApp.hpp"
#include "StelUtils.hpp"
#include "StelPainter.hpp"

#include <QImageReader>
#include <QSize>
#include <QDebug>
#include <QUrl>
#include <QImage>
#include <QNetworkReply>
#include <QtEndian>
#include <QFuture>
#include <QtConcurrent>

StelTexture::StelTexture() : networkReply(NULL), loader(NULL), errorOccured(false), id(0), avgLuminance(-1.f)
{
	width = -1;
	height = -1;
	initializeOpenGLFunctions();
}

StelTexture::~StelTexture()
{
	if (id != 0)
	{
		if (glIsTexture(id)==GL_FALSE)
		{
			qDebug() << "WARNING: in StelTexture::~StelTexture() tried to delete invalid texture with ID=" << id << " Current GL ERROR status is " << glGetError();
		}
		else
		{
			glDeleteTextures(1, &id);
		}
		id = 0;
	}
	if (networkReply != NULL)
	{
		networkReply->abort();
		networkReply->deleteLater();
	}
	if (loader != NULL) {
		delete loader;
		loader = NULL;
	}
}

/*************************************************************************
 This method should be called if the texture loading failed for any reasons
 *************************************************************************/
void StelTexture::reportError(const QString& aerrorMessage)
{
	errorOccured = true;
	errorMessage = aerrorMessage;
	// Report failure of texture loading
	emit(loadingProcessFinished(true));
}

/*************************************************************************
 Defined to be passed to QtConcurrent::run
 *************************************************************************/
static QImage loadFromPath(const QString &path)
{
	return QImage(path);
}

static QImage loadFromData(const QByteArray& data)
{
	return QImage::fromData(data);
}

/*************************************************************************
 Bind the texture so that it can be used for openGL drawing (calls glBindTexture)
 *************************************************************************/

bool StelTexture::bind()
{
	if (id != 0)
	{
		// The texture is already fully loaded, just bind and return true;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, id);
		return true;
	}
	if (errorOccured)
		return false;

	// If the file is remote, start a network connection.
	if (loader == NULL && networkReply == NULL && fullPath.startsWith("http://")) {
		QNetworkRequest req = QNetworkRequest(QUrl(fullPath));
		// Define that preference should be given to cached files (no etag checks)
		req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
		req.setRawHeader("User-Agent", StelUtils::getApplicationName().toLatin1());
		networkReply = StelApp::getInstance().getNetworkAccessManager()->get(req);
		connect(networkReply, SIGNAL(finished()), this, SLOT(onNetworkReply()));
		return false;
	}
	// The network connection is still running.
	if (networkReply != NULL)
		return false;
	// Not a remote file, start a loader from local file.
	if (loader == NULL)
	{
		loader = new QFuture<QImage>(QtConcurrent::run(loadFromPath, fullPath));
		return false;
	}
	// Wait until the loader finish.
	if (!loader->isFinished())
		return false;
	// Finally load the data in the main thread.
	glLoad(loader->result());
	delete loader;
	loader = NULL;
	return true;
}

void StelTexture::onNetworkReply()
{
	Q_ASSERT(loader == NULL);
	if (networkReply->error() != QNetworkReply::NoError)
	{
		reportError(networkReply->errorString());
	}
	else
	{
		QByteArray data = networkReply->readAll();
		loader = new QFuture<QImage>(QtConcurrent::run(loadFromData, data));
	}
	networkReply->deleteLater();
	networkReply = NULL;
}

/*************************************************************************
 Return the width and heigth of the texture in pixels
*************************************************************************/
bool StelTexture::getDimensions(int &awidth, int &aheight)
{
	if (width<0 || height<0)
	{
		// Try to get the size from the file without loading data
		QImageReader im(fullPath);
		if (!im.canRead())
		{
			return false;
		}
		QSize size = im.size();
		width = size.width();
		height = size.height();
	}
	awidth = width;
	aheight = height;
	return true;
}

QByteArray StelTexture::convertToGLFormat(const QImage& image, GLint *format, GLint *type)
{
	QByteArray ret;
	const int width = image.width();
	const int height = image.height();
	if (image.isGrayscale())
	{
		*format = image.hasAlphaChannel() ? GL_LUMINANCE_ALPHA : GL_LUMINANCE;
	}
	else if (image.hasAlphaChannel())
	{
		*format = GL_RGBA;
	}
	else
		*format = GL_RGB;
	*type = GL_UNSIGNED_BYTE;
	int bpp = *format == GL_LUMINANCE_ALPHA ? 2 :
			  *format == GL_LUMINANCE ? 1 :
			  *format == GL_RGBA ? 4 :
			  3;

	ret.reserve(width * height * bpp);
	QImage tmp = image.convertToFormat(QImage::Format_ARGB32);

	// flips bits over y
	int ipl = tmp.bytesPerLine() / 4;
	for (int y = 0; y < height / 2; ++y)
	{
		int *a = (int *) tmp.scanLine(y);
		int *b = (int *) tmp.scanLine(height - y - 1);
		for (int x = 0; x < ipl; ++x)
			qSwap(a[x], b[x]);
	}

	// convert data
	for (int i = 0; i < height; ++i)
	{
		uint *p = (uint *) tmp.scanLine(i);
		for (int x = 0; x < width; ++x)
		{
			uint c = qToBigEndian(p[x]);
			const char* ptr = (const char*)&c;
			switch (*format)
			{
			case GL_RGBA:
				ret.append(ptr + 1, 3);
				ret.append(ptr, 1);
				break;
			case GL_RGB:
				ret.append(ptr + 1, 3);
				break;
			case GL_LUMINANCE:
				ret.append(ptr + 1, 1);
				break;
			case GL_LUMINANCE_ALPHA:
				ret.append(ptr + 1, 1);
				ret.append(ptr, 1);
				break;
			default:
				Q_ASSERT(false);
			}
		}
	}
	return ret;
}


// Actually load the texture to openGL memory
bool StelTexture::glLoad(const QImage& image)
{
	if (image.isNull())
	{
		reportError("Unknown error");
		return false;
	}
	width = image.width();
	height = image.height();
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, loadParams.filtering);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, loadParams.filtering);

	GLint format, type;
	QByteArray data = convertToGLFormat(image, &format, &type);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
				 type, data.constData());
	bool genMipmap = false;
	if (genMipmap)
		glGenerateMipmap(GL_TEXTURE_2D);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, loadParams.wrapMode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, loadParams.wrapMode);

	// Report success of texture loading
	emit(loadingProcessFinished(false));
	return true;
}
