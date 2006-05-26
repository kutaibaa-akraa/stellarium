/*
 * Author and Copyright of this file and of the stellarium telescope feature:
 * Johannes Gajdosik, 2006
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

#ifndef _TELESCOPE_MGR_H_
#define _TELESCOPE_MGR_H_


#include "fader.h"
#include "vecmath.h"

#include <vector>
#include <map>
#include <string>

using namespace std;

class InitParser;
class Projector;
class Navigator;
class s_font;
class s_texture;
class StelObject;
class Telescope;

class TelescopeMgr {
public:
  TelescopeMgr(void);
  virtual ~TelescopeMgr(void);
  void init(const InitParser &conf);
  void update(int delta_time);
  void draw(const Projector *prj,const Navigator *nav) const;
  void communicate(void);
   
  void set_names_fade_duration(float duration)
    {name_fader.set_duration((int) (duration * 1000.f));}
   
  vector<StelObject*> search_around(Vec3d v,double lim_fov) const;
  
  StelObject *searchByNameI18n(const wstring &nameI18n) const;
  
  std::vector<wstring> listMatchingObjectsI18n(const wstring& objPrefix,
                                               unsigned int maxNbItem) const;
  
  void set_label_color(const Vec3f &c) {label_color = c;}
  const Vec3f &getLabelColor(void) const {return label_color;}

  void set_circle_color(const Vec3f &c) {circle_color = c;}
  const Vec3f &getCircleColor(void) const {return circle_color;}
  
  //! Set display flag for Telescopes
  void setFlagTelescopes(bool b) {telescope_fader=b;}
  //! Get display flag for Telescopes
  bool getFlagTelescopes(void) const {return (bool)telescope_fader;}  
  
  //! Set display flag for Telescope names
  void setFlagTelescopeName(bool b) {name_fader=b;}
  //! Get display flag for Telescope names
  bool getFlagTelescopeName(void) const {return name_fader==true;}
  
  //! Define font file name and size to use for telescope names display
  void setFont(float font_size,const string &font_name);

  //! send a J2000-goto-command to the specified telescope
  void telescopeGoto(int telescope_nr,const Vec3d &j2000_pos);
private:
#ifdef WIN32
  bool wsa_ok;
#endif
  LinearFader name_fader;
  LinearFader telescope_fader;
  Vec3f circle_color;
  Vec3f label_color;
  s_font *telescope_font;
  s_texture *telescope_texture;

  class TelescopeMap : public std::map<int,Telescope*> {
  public:
    ~TelescopeMap(void) {clear();}
    void clear(void);
  };
  TelescopeMap telescope_map;
};


#endif
