/*
 * Stellarium
 * Copyright (C) 2003 Fabien Ch�reau
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "stdio.h"
#include "projector.h"


Projector::Projector(int _screenW, int _screenH, double _fov, double _min_fov, double _max_fov) :
	min_fov(_min_fov), max_fov(_max_fov), zNear(0.1), zFar(10000)
{
	set_fov(_fov);
	set_screen_size(_screenW,_screenH);
}

Projector::~Projector()
{
}

void Projector::set_fov(double f)
{
	fov = f;
	if (f>max_fov) fov = max_fov;
	if (f<min_fov) fov = min_fov;
	init_project_matrix();
}


void Projector::set_square_viewport(void)
{
	glDisable(GL_STENCIL_TEST);
	int mind = MY_MIN(screenW,screenH);
	set_viewport((screenW-mind)/2, (screenH-mind)/2, mind, mind);
}

void Projector::set_disk_viewport(void)
{
	set_square_viewport();
    glEnable(GL_STENCIL_TEST);
	glClear(GL_STENCIL_BUFFER_BIT);
 	glStencilFunc(GL_ALWAYS, 0x1, 0x1);
    glStencilOp(GL_ZERO, GL_REPLACE, GL_REPLACE);

	// Draw the disk in the stencil buffer
	set_2Dfullscreen_projection();
	glTranslatef(screenW/2,screenH/2,0.f);
	GLUquadricObj * p = gluNewQuadric();
	gluDisk(p, 0., MY_MIN(screenW,screenH)/2, 128, 1);
	gluDeleteQuadric(p);
	restore_from_2Dfullscreen_projection();

	glStencilFunc(GL_EQUAL, 0x1, 0x1);
}

void Projector::set_viewport(int x, int y, int w, int h)
{
	glDisable(GL_STENCIL_TEST);
	vec_viewport[0] = x;
	vec_viewport[1] = y;
	vec_viewport[2] = w;
	vec_viewport[3] = h;
	glViewport(x, y, w, h);
	ratio = (float)h/w;
	init_project_matrix();
}

void Projector::set_screen_size(int w, int h)
{
	screenW = w;
	screenH = h;
}

void Projector::set_clipping_planes(double znear, double zfar)
{
	zNear = znear;
	zFar = zfar;
	init_project_matrix();
}

void Projector::change_fov(double deltaFov)
{
    // if we are zooming in or out
    if (deltaFov)
    {
		if (fov+deltaFov>min_fov && fov+deltaFov<max_fov)  fov+=deltaFov;
		if (fov+deltaFov>max_fov) fov=max_fov;
		if (fov+deltaFov<min_fov) fov=min_fov;
		init_project_matrix();
    }
}

// Init the viewing matrix, setting the field of view, the clipping planes, and screen ratio
// The function is a reimplementation of gluPerspective
void Projector::init_project_matrix(void)
{
	double f = 1./tan(fov*M_PI/360.);
	mat_projection = Mat4d(	f*ratio, 0., 0., 0.,
							0., f, 0., 0.,
							0., 0., (zFar + zNear)/(zNear - zFar), -1.,
							0., 0., (2.*zFar*zNear)/(zNear - zFar), 0.);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(mat_projection);
    glMatrixMode(GL_MODELVIEW);
}

void Projector::update_openGL(void) const
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(mat_projection);
    glMatrixMode(GL_MODELVIEW);
	glViewport(vec_viewport[0], vec_viewport[1], vec_viewport[2], vec_viewport[3]);
}

// Set the standard modelview matrices used for projection
void Projector::set_modelview_matrices(	const Mat4d& _mat_earth_equ_to_eye,
										const Mat4d& _mat_helio_to_eye,
										const Mat4d& _mat_local_to_eye)
{
	mat_earth_equ_to_eye = _mat_earth_equ_to_eye;
	mat_helio_to_eye = _mat_helio_to_eye;
	mat_local_to_eye = _mat_local_to_eye;

	inv_mat_earth_equ_to_eye = (mat_projection*mat_earth_equ_to_eye).inverse();
	inv_mat_helio_to_eye = (mat_projection*mat_helio_to_eye).inverse();
	inv_mat_local_to_eye = (mat_projection*mat_local_to_eye).inverse();
}


bool Projector::project_custom(const Vec3d& v, Vec3d& win, const Mat4d& mat) const
{
    gluProject(v[0],v[1],v[2],mat,mat_projection,vec_viewport,&win[0],&win[1],&win[2]);
	return (win[2]<1.);
}

void Projector::unproject_custom(double x ,double y, Vec3d& v, const Mat4d& mat) const
{
	gluUnProject(x,y,1.,mat,mat_projection,vec_viewport,&v[0],&v[1],&v[2]);
}

// Set the drawing mode in 2D for drawing in the full screen
// Use reset_perspective_projection() to restore previous projection mode
void Projector::set_2Dfullscreen_projection(void) const
{
	glViewport(0, 0, screenW, screenH);
	glMatrixMode(GL_PROJECTION);		// projection matrix mode
    glPushMatrix();						// store previous matrix
    glLoadIdentity();
    gluOrtho2D(	0, screenW,
				0, screenH);			// set a 2D orthographic projection
	glMatrixMode(GL_MODELVIEW);			// modelview matrix mode

    glPushMatrix();
    glLoadIdentity();
}

// Reset the previous projection mode after a call to set_orthographic_projection()
void Projector::restore_from_2Dfullscreen_projection(void) const
{
    glMatrixMode(GL_PROJECTION);		// Restore previous matrix
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
	glViewport(vec_viewport[0], vec_viewport[1], vec_viewport[2], vec_viewport[3]);
    glPopMatrix();
}

// Set the drawing mode in 2D. Use reset_perspective_projection() to reset
// previous projection mode
void Projector::set_orthographic_projection(void) const
{
	glMatrixMode(GL_PROJECTION);		// projection matrix mode
    glPushMatrix();						// store previous matrix
    glLoadIdentity();
    gluOrtho2D(	vec_viewport[0], vec_viewport[0] + vec_viewport[2],
				vec_viewport[1], vec_viewport[1] + vec_viewport[3]);	// set a 2D orthographic projection
	glMatrixMode(GL_MODELVIEW);			// modelview matrix mode
    glPushMatrix();
    glLoadIdentity();
}

// Reset the previous projection mode after a call to set_orthographic_projection()
void Projector::reset_perspective_projection(void) const
{
    glMatrixMode(GL_PROJECTION);		// Restore previous matrix
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}



// Reimplementation of gluSphere : glu is overrided for non standard projection
void Projector::sSphere(GLdouble radius, GLint slices, GLint stacks, const Mat4d& mat, int orient_inside) const
{
	glPushMatrix();
	glLoadMatrixd(mat);
	GLUquadricObj * p = gluNewQuadric();
	gluQuadricTexture(p,GL_TRUE);
	if (orient_inside) gluQuadricOrientation(p, GLU_INSIDE);
	gluSphere(p, radius, slices, stacks);
	gluDeleteQuadric(p);
	glPopMatrix();
}
