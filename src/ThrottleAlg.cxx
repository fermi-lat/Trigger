/**
 * @file ThrottleAlg.cxx
 * @brief implementation for class ThrottleAlg

 $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/ThrottleAlg.cxx,v 1.2 2004/09/08 18:13:09 burnett Exp $
 * @author David Wren - dnwren@milkyway.gsfc.nasa.gov
*/
#include "ThrottleAlg.h"
#include "enums/TriggerBits.h"
#include <cmath>
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
ThrottleAlg::ThrottleAlg(){
}

void ThrottleAlg::setup(){

	// initialize some variables
    m_number_triggered = 0;
	m_triggered_towers = 0;
	m_throttle = false;

    return;
}

unsigned int ThrottleAlg::calculate(const Event::EventHeader& header, 
							const Event::TkrDigiCol& tkr, 
							const Event::AcdDigiCol& acd,  double threshold)
{

	//Get the list of triggered towers, and set the lists of acd tiles hit
	getTriggeredTowers(tkr);
	if (m_number_triggered == 0) return 0;
	setACDTileList(acd, threshold);
	
	//check the list of triggered towers against hit acd tiles
	bool done = false;
	while (!done){
		done = compare();
	}
	if (m_throttle){
        m_throttle=false;
		return enums::b_THROTTLE;
	}
	
	return 0;
}

void ThrottleAlg::getTriggeredTowers(const Event::TkrDigiCol& planes)
{
    // purpose and method: determine which towers have 3-in-a-rows

    using namespace Event;

    // define a map to sort hit planes according to tower and plane axis
    typedef std::pair<idents::TowerId, idents::GlastAxis::axis> Key;
    typedef std::map<Key, unsigned int> Map;
    Map layer_bits;
    
    // this loop sorts the hits by setting appropriate bits in the tower-plane hit map
    for( Event::TkrDigiCol::const_iterator it = planes.begin(); it != planes.end(); ++it){
        const TkrDigi& t = **it;
        layer_bits[std::make_pair(t.getTower(), t.getView())] |= layer_bit(t.getBilayer());
    }
    
    // now look for a three in a row in x-y coincidence
    for( Map::iterator itr = layer_bits.begin(); itr !=layer_bits.end(); ++ itr){

        Key theKey=(*itr).first;
        idents::TowerId tower = theKey.first;
        if( theKey.second==idents::GlastAxis::X) { 
            // found an x: and with corresponding Y (if exists)
            unsigned int 
                xbits = (*itr).second,
                ybits = layer_bits[std::make_pair(tower, idents::GlastAxis::Y)];

            if( three_in_a_row( xbits & ybits) ){
               m_number_triggered++; //counts the number of towers triggered
			   m_triggered_towers |= int(pow(2,tower.id()));//adds the tower
            }
        }
    }

    return;
}

void ThrottleAlg::setACDTileList(const Event::AcdDigiCol& digiCol,  double threshold)
{
	// Code in AcdReconAlg shows how to get the id of a tile that
	// is hit.  So get the id, and just set it in one of 3 lists.
	// Two lists will be for all the side tiles (16x4), and
	// one will be for the top tiles (25 of them).

	//initialize the acd tile lists
	m_acdtop = 0;
	m_acdX = 0;
	m_acdY = 0;

	short s_face;
	short s_tile;
	short s_row;
	short s_column;

    Event::AcdDigiCol::const_iterator acdDigiIt;
    
	//loop over all tiles
    for (acdDigiIt = digiCol.begin(); acdDigiIt != digiCol.end(); ++acdDigiIt) {
        
        idents::AcdId id = (*acdDigiIt)->getId();

        // if it is a tile...
		if (id.tile()==true) {
        // toss out hits below threshold
        if ((*acdDigiIt)->getEnergy() < threshold) continue;
           //get the face, row, and column numbers
		   s_face = id.face();
		   s_row  = id.row();
		   s_column = id.column();
           //set the tile number (depending on the face)
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
		   //set the appropriate bit for the tile number
		   switch (s_face) {
		      case 0: //top
				 m_acdtop |= int(pow(2,s_tile));
                 break;
              case 1: //-X
                 m_acdX |= int(pow(2,s_tile+16));
                 break;
              case 2: //-Y
                 m_acdY |= int(pow(2,s_tile+16));
			     break;
              case 3: //+X
                 m_acdX |= int(pow(2,s_tile));
                 break;
              case 4: //+Y
                 m_acdY |= int(pow(2,s_tile));
                 break;
           }//switch
		}//if (id.tile())
	}//for loop over tiles
    return;
}

 	
int ThrottleAlg::getTowerID()
{
   //looks at the list of triggered towers, and gets the first tower
   //to look at.  
    if (m_triggered_towers & 0x1) return 0;
	if (m_triggered_towers & 0x2) return 1;
    if (m_triggered_towers & 0x4) return 2;
	if (m_triggered_towers & 0x8) return 3;
	if (m_triggered_towers & 0x10) return 4;
	if (m_triggered_towers & 0x20) return 5;
	if (m_triggered_towers & 0x40) return 6;
	if (m_triggered_towers & 0x80) return 7;
	if (m_triggered_towers & 0x100) return 8;
	if (m_triggered_towers & 0x200) return 9;
	if (m_triggered_towers & 0x400) return 10;
	if (m_triggered_towers & 0x800) return 11;
	if (m_triggered_towers & 0x1000) return 12;
	if (m_triggered_towers & 0x2000) return 13;
	if (m_triggered_towers & 0x4000) return 14;
	if (m_triggered_towers & 0x8000) return 15;
	
	return -1; //if there are no matches
}

void ThrottleAlg::getMask(int towerID)
{
	m_maskTop = 0;
	m_maskX = 0;
	m_maskY = 0;

	//  If using the top 2 rows of the acd, use up to 12 tiles
	switch (towerID)
	{
	case 0://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x318000;//0000 0000 0011 0001 1000 0000 0000 0000
		m_maskX = 0x03180000;//0000 0011 0001 1000 0000 0000 0000 0000
		m_maskY = 0x630000;  //0000 0000 0110 0011 0000 0000 0000 0000
		return;
    case 1://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C00; //0000 0000 0000 0001 1000 1100 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0xC60000;  //0000 0000 1100 0110 0000 0000 0000 0000
		return;
	case 2://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC60;   //0000 0000 0000 0000 0000 1100 0110 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C0000; //0000 0001 1000 1100 0000 0000 0000 0000
		return;
	case 3://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x63;    //0000 0000 0000 0000 0000 0000 0110 0011
		m_maskX = 0x63;      //0000 0000 0000 0000 0000 0000 0110 0011
		m_maskY = 0x3180000; //0000 0011 0001 1000 0000 0000 0000 0000
		return;
	case 4://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x630000;//0000 0000 0110 0011 0000 0000 0000 0000
		m_maskX = 0x18C0000; //0000 0001 1000 1100 0000 0000 0000 0000
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
		m_maskX = 0xC6;      //0000 0000 0000 0000 0000 0000 1100 0110
		m_maskY = 0;         //0
		return;
	case 8://                      28   24   20   16   12   8    4    0
		m_maskTop = 0xC60000; //0000 0000 1100 0110 0000 0000 0000 0000
		m_maskX = 0xC60000;   //0000 0000 1100 0110 0000 0000 0000 0000
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
		m_maskX = 0x18C;     //0000 0000 0000 0000 0000 0001 1000 1100
		m_maskY = 0;         //0
		return;
	case 12://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0000;//0000 0001 1000 1100 0000 0000 0000 0000
		m_maskX = 0x630000;   //0000 0000 0110 0011 0000 0000 0000 0000
		m_maskY = 0x318;      //0000 0000 0000 0000 0000 0011 0001 1000
		return;
	case 13://                    28   24   20   16   12   8    4    0
		m_maskTop = 0xC6000; //0000 0000 0000 1100 0110 0000 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C;     //0000 0000 0000 0000 0000 0001 1000 1100
		return;
    case 14://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x6300;  //0000 0000 0000 0000 0110 0011 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0xC6;      //0000 0000 0000 0000 0000 0000 1100 0110
		return;
    case 15://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x318;   //0000 0000 0000 0000 0000 0011 0001 1000
		m_maskX = 0x318;     //0000 0000 0000 0000 0000 0011 0001 1000
		m_maskY = 0x63;      //0000 0000 0000 0000 0000 0000 0110 0011
		return;
	}
	return;
}

void ThrottleAlg::removeTower(int twr)
{
	if (twr==0){m_triggered_towers ^= 0x1; return;}
	if (twr==1){m_triggered_towers ^= 0x2; return;}
	if (twr==2){m_triggered_towers ^= 0x4; return;}
	if (twr==3){m_triggered_towers ^= 0x8; return;}
	if (twr==4){m_triggered_towers ^= 0x10; return;}
	if (twr==5){m_triggered_towers ^= 0x20; return;}
	if (twr==6){m_triggered_towers ^= 0x40; return;}
	if (twr==7){m_triggered_towers ^= 0x80; return;}
	if (twr==8){m_triggered_towers ^= 0x100; return;}
	if (twr==9){m_triggered_towers ^= 0x200; return;}
	if (twr==10){m_triggered_towers ^= 0x400; return;}
	if (twr==11){m_triggered_towers ^= 0x800; return;}
	if (twr==12){m_triggered_towers ^= 0x1000; return;}
	if (twr==13){m_triggered_towers ^= 0x2000; return;}
	if (twr==14){m_triggered_towers ^= 0x4000; return;}
	if (twr==15){m_triggered_towers ^= 0x8000; return;}
	
	return;
}

bool ThrottleAlg::compare()
{
   int  tower = 0;
   bool done = true;
   bool notdone = false;

   if ((tower = getTowerID()) == -1) return done;//no more towers
   getMask(tower);
   removeTower(tower);
   if ( (m_acdtop & m_maskTop)||(m_acdX & m_maskX)||(m_acdY & m_maskY) )
   {
       m_throttle = true;
	   return done;
   }
   if (m_triggered_towers==0) return done;
   return notdone;
}

/*
Throttle Documentation

The throttle is designed to activate if a triggered tower is next to an ACD tile
that is above its veto threshold (it is hit).  We only consider the top face of
the ACD, and the top two rows on the sides.  This can easily be changed if we 
want to consider the top 3 rows of the ACD, and I included the appropriate
bit masks in the code (commented out).

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

*/
