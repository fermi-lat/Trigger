/**
 * @file ThrottleAlg.h
 * @brief header for class ThrottleAlg

 $Header$
 * @author David Wren - dnwren@milkyway.gsfc.nasa.gov
*/

#ifndef THROTTLE_ALG_H
#define THROTTLE_ALG_H

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"


#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"

#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"

#include "CLHEP/Geometry/Point3D.h"
#include "CLHEP/Vector/LorentzVector.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/AcdDigi.h"

#include <map>
#include <cassert>

namespace {
    inline bool three_in_a_row(unsigned bits)
    {
        while( bits ) {
            if( (bits & 7) ==7) return true;
            bits >>= 1;
        }
        return false;
    }
    inline unsigned layer_bit(int layer){ return 1 << layer;}
}


/** @class ThrottleAlg
@brief alg that determines whether the throttle is activated.

* @author David Wren - dnwren@milkyway.gsfc.nasa.gov

*/
class ThrottleAlg{
public:
    ThrottleAlg();
    //! set parameters and attach to various perhaps useful services.
    void setup();
    //! process one event
    unsigned int ThrottleAlg::calculate(const Event::EventHeader& header, 
							            const Event::TkrDigiCol& tkr, 
							            const Event::AcdDigiCol& acd,  double threshold);
	// returns THROTTLE_SET if the throttle was set
private: 
    //! determine tracker trigger bits
    //! sets the triggered towers in the form of an unsigned short
	void getTriggeredTowers(const Event::TkrDigiCol& planes);
    
	//! sets the list(s) of hit ACD tiles
	void setACDTileList(const Event::AcdDigiCol& digiCol,  double threshold);
       
	//! sets the id of the next tower to look at 
    //@ return the tower id
	int getTowerID();
    
	//! sets the correct mask for the triggered tower
    void getMask(int towerID);
    
	//! remove the tower from the list, becuase it has been examined
	void removeTower(int twr);

	//! gets the tower ID and mask, then calls the functions
	//! that do the checking
	bool compare();

    /// access to the Glast Detector Service to read in geometry constants from XML files
    IGlastDetSvc *m_glastDetSvc;

	bool m_throttle;
    unsigned short m_triggered_towers, m_number_triggered;
	unsigned int m_trigger_word, m_acdtop, m_acdX, m_acdY,
		         m_maskTop, m_maskX, m_maskY;
	const static unsigned int THROTTLE_SET = 32;

};


/** @page throttledoc Throttle Documentation

@verbatim

author: David Wren, dnwren@milkyway.gsfc.nasa.gov
date: 9 March 2004

The throttle is designed to activate if a triggered tower is next to an ACD tile
that is above its veto threshold (it is hit).  We only consider the top face of
the ACD, and the top two rows on the sides.  This can easily be changed if we 
want to consider the top 3 rows of the ACD.  I've included the appropriate bit
masks for this at the end of this documentation.

First, we read in the list of tiles that are hit from AcdDigi, which gives a 
face, a row, and a column.  The format of the ACD digi Id is described in 
AcdId.h:
  __ __       __ __ __     __ __ __ __      __ __ __ __
  LAYER         FACE           ROW            COLUMN
                 OR                             OR
           RIBBON ORIENTATION               RIBBON NUMBER

The face codes are:
Face 0 == top  (hat)
Face 1 == -X side 
Face 2 == -Y side
Face 3 == +X side
Face 4 == +Y side
Face 5, 6 == ribbon

For the top, the tile-tower association looks like this (where lines indicate
the tower-tile associations we are using):

Tile     24    19    14    9	 4    /|\ +Y 
           \  /  \  /  \  /  \  /      |  
Tower	    12    13    14    15       | 
           /  \  /  \  /  \  /  \      |
Tile	 23    18    13	   8	 3     |________\
           \  /  \  /  \  /  \  /               /
Tower       8     9     10    11               +X
           /  \  /  \  /  \  /  \
Tile     22    17    12    7	 2
           \  /  \  /  \  /  \  /
Tower       4     5     6     7
           /  \  /  \  /  \  /  \
Tile     21    16    11    6     1
           \  /  \  /  \  /  \  /
Tower       0     1     2     3
           /  \  /  \  /  \  /  \
Tile     20    15    10    5     0


For face 1 (-X side), the tile tower association looks like this:

ACD Column    4      3     2      1     0
                
                tower tower tower tower
                 12     8     4     0 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---

So tower 12, for example, is shadowed by tiles 0, 1, 5, 6, 10, 11, and 15.
The column and row designations come from the AcdId designation.


For face 2 (-Y side), the tile tower association looks like this:

ACD Column    0      1     2      3     4
                
                tower tower tower tower
                  0     1     2     3 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---


For face 3 (+X side), the tile tower association looks like this:

ACD Column    0      1     2      3     4
                
                tower tower tower tower
                  3     7     11     15 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---


For face 4 (+Y side), the tile tower association looks like this:

ACD Column    4      3     2      1     0
                
                tower tower tower tower
                 15     14    13     12 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---

So depending on which ACD rows we are considering for the throttle, we have 
different numbers of tiles associated with a triggered tower:

Tower location           Number of Rows Considered       Number of Tiles

Center                             2                           4
                                   3                           4 

Edge                               2                           8
                                   3                           10

Corner                             2                           12
                                   3                           16


In the code, the formula for going from the AcdId.h info to a tile number is this:

		   if (s_face == 0) 
			   s_tile = (4-s_column)*5 + s_row;
		   else if ((s_face == 2) || (s_face == 3)){
			   if (s_row < 3) s_tile = s_row*5 + s_column;
		       else           s_tile = 15;
		   }
		   else if ((s_face == 1) || (s_face == 4)){
			   if (s_row < 3) s_tile = s_row*5 + (4-s_column);
		       else           s_tile = 15;
		   }

The algorithm first creates the list of ACD tiles that are hit,
and a list of towers that are triggered.  From the list of triggered
towers, it looks up bit masks of tiles to check for that tower.  
These masks are hard coded according to the following scheme:

The top ACD tiles are represented by a 32 bit words: m_maskTop or 
m_acdtop, where "mask" indicates that the word is the mask of bits
that corresponds to a given tower, and "acd" indicates the list of 
hit ACD tiles.
               1111 1111 1111 1111 
bit position   fedc ba98 7654 3210 fedc ba98 7654 3210
               ---------------------------------------
                       1 1111 1111 
ACD tile (hex)         8 7654 3210 fedc ba98 7654 3210                    

The side ACD tiles are represented by one of two 32 bit words.  The
two words each contain information about ACD tiles that are in one
plane (X or Y).  The 16 least significant bits hold information on
the "+" faces (X+ or Y+), while the 16 most significant bits hold
information on the "-" faces (X- or Y-).

m_acdX and m_maskX are arranged like this:
               1111 1111 1111 1111 
bit position   fedc ba98 7654 3210 fedc ba98 7654 3210
               ---------------------------------------                        
ACD tile (hex)  edc ba98 7654 3210  edc ba98 7654 3210
                     X- face             X+ face

m_acdY and m_maskY are arranged similarly:
               1111 1111 1111 1111 
bit position   fedc ba98 7654 3210 fedc ba98 7654 3210
               ---------------------------------------                        
ACD tile (hex)  edc ba98 7654 3210  edc ba98 7654 3210
                     Y- face             Y+ face

ANDing the appropriate masks with the tile lists allows us to
tell if a hit tile corresponds to a triggered tower:

if ( (m_acdtop & m_maskTop)||(m_acdX & m_maskX)||(m_acdY & m_maskY) )

If this condition is met, the throttle is activated.
----------------------------------------------------------------------

If the user wants to use the top of the acd and the top 3 rows
of the side tiles (instead of the top 2 rows), the masks in
TriggerAlg::getMask(int towerID) can be replaced with the set of masks
below.

    //If using the acd top and top 3 side rows of the acd, use up 
    //to 16 tiles for each tower:
	switch (towerID)
	{
	case 0://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x318000;//0000 0000 0011 0001 1000 0000 0000 0000
		m_maskX = 0x63180000;//0110 0011 0001 1000 0000 0000 0000 0000
		m_maskY = 0x0C630000;//0000 1100 0110 0011 0000 0000 0000 0000
		return;
    case 1://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C00; //0000 0000 0000 0001 1000 1100 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C60000;//0001 1000 1100 0110 0000 0000 0000 0000
		return;
	case 2://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC60;   //0000 0000 0000 0000 0000 1100 0110 0000
		m_maskX = 0;         //0
		m_maskY = 0x318C0000;//0011 0001 1000 1100 0000 0000 0000 0000
		return;
	case 3://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x63;    //0000 0000 0000 0000 0000 0000 0110 0011
		m_maskX = 0xC63;     //0000 0000 0000 0000 0000 1100 0110 0011
		m_maskY = 0x63180000;//0110 0011 0001 1000 0000 0000 0000 0000
		return;
	case 4://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x630000;//0000 0000 0110 0011 0000 0000 0000 0000
		m_maskX = 0x318C0000;//0011 0001 1000 1100 0000 0000 0000 0000
		m_maskY = 0;         //0
		return;
	case 5://                      28   24   20   16   12   8    4    0
		m_maskTop = 0x31800;  //0000 0000 0000 0011 0001 1000 0000 0000
		m_maskX = 0;          //0
		m_maskY = 0;          //0
	    return;
	case 6://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0;  //0000 0000 0000 0000 0001 1000 1100 0000
		m_maskX = 0;         //0
		m_maskY = 0;         //0
		return;
	case 7://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC6;    //0000 0000 0000 0000 0000 0000 1100 0110
		m_maskX = 0x18C6;    //0000 0000 0000 0000 0001 1000 1100 0110
		m_maskY = 0;         //0
		return;
	case 8://                      28   24   20   16   12   8    4    0
		m_maskTop = 0xC60000; //0000 0000 1100 0110 0000 0000 0000 0000
		m_maskX = 0x18C60000; //0001 1000 1100 0110 0000 0000 0000 0000
		m_maskY = 0;          //0
		return;
	case 9://                      28   24   20   16   12   8    4    0
		m_maskTop = 0x63000;  //0000 0000 0000 0110 0011 0000 0000 0000
		m_maskX = 0;          //0
		m_maskY = 0;          //0
		return;
	case 10://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x3180;  //0000 0000 0000 0000 0011 0001 1000 0000
		m_maskX = 0;         //0 
		m_maskY = 0;         //0 
		return;
	case 11://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x18C;   //0000 0000 0000 0000 0000 0001 1000 1100
		m_maskX = 0x318C;    //0000 0000 0000 0000 0011 0001 1000 1100
		m_maskY = 0;         //0
		return;
	case 12://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0000;//0000 0001 1000 1100 0000 0000 0000 0000
		m_maskX = 0xC630000;  //0000 1100 0110 0011 0000 0000 0000 0000
		m_maskY = 0xC318;     //0000 0000 0000 0000 0110 0011 0001 1000
		return;
	case 13://                    28   24   20   16   12   8    4    0
		m_maskTop = 0xC6000; //0000 0000 0000 1100 0110 0000 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x318C;    //0000 0000 0000 0000 0011 0001 1000 1100
		return;
    case 14://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x6300;  //0000 0000 0000 0000 0110 0011 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C6;    //0000 0000 0000 0000 0001 1000 1100 0110
		return;
    case 15://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x318;   //0000 0000 0000 0000 0000 0011 0001 1000
		m_maskX = 0x6318;    //0000 0000 0000 0000 0110 0011 0001 1000
		m_maskY = 0x0C63;    //0000 0000 0000 0000 0000 1100 0110 0011
		return;
	}
        @endverbatim
*/

#endif

