
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "Vec3.h"
#include "Mat3.h"
//#include <string.h>

#include "Grid.h"

// ================= MACROS

//#define i3D( ix, iy, iz )  ( iz*nxy + iy*nx + ix  ) 

// ================= CONSTANTS

// ================= GLOBAL VARIABLES

GridShape gridShape;
double *  gridV;

const static int images [27][3] = {
	{ 0, 0, 0}, {-1, 0, 0}, {+1, 0, 0},
	{ 0,-1, 0}, {-1,-1, 0}, {+1,-1, 0},
	{ 0,+1, 0}, {-1,+1, 0}, {+1,+1, 0},
	{ 0, 0,-1}, {-1, 0,-1}, {+1, 0,-1},
	{ 0,-1,-1}, {-1,-1,-1}, {+1,-1,-1},
	{ 0,+1,-1}, {-1,+1,-1}, {+1,+1,-1},
	{ 0, 0,+1}, {-1, 0,+1}, {+1, 0,+1},
	{ 0,-1,+1}, {-1,-1,+1}, {+1,-1,+1},
	{ 0,+1,+1}, {-1,+1,+1}, {+1,+1,+1}
};


//#define   NEXTBIT( index, OPERATION ) { if(bitmask_&bitmask){ OPERATION; index++; bitmask_=bitmask_<<1; } }
#define   NEXTBIT( index, OPERATION ) { if(bitmask&1){ OPERATION; index++; bitmask=bitmask>>1; } }

inline int getMultipoleBasis( Vec3d pos, double * bas, uint32_t bitmask ){
    // https://en.wikipedia.org/wiki/Multipole_expansion#Expansion_in_Cartesian_coordinates
    //uint32_t bitmask_ = 1;
    int i = 0;
    double ir2 = 1/pos.norm2();
    double ir  = sqrt(ir2);
    bas[i]     = ir;              
    double ir3 = ir *ir2;
    NEXTBIT( i, bas[i] = ir3*pos.z; )    
    NEXTBIT( i, bas[i] = ir3*pos.y; )    
    NEXTBIT( i, bas[i] = ir3*pos.x; )    
    // problem is they are linearily dependent => we have to use spherical 
    Vec3d ir5p; ir5p.set_mul( pos, ir3*ir2 ); 
    NEXTBIT( i, bas[i] = 2*ir5p.z*pos.z - ir5p.y*pos.y - ir5p.x*pos.x;    )    
    NEXTBIT( i, bas[i] =   ir5p.x*pos.x - ir5p.y*pos.y;                   )    
    NEXTBIT( i, bas[i] =   ir5p.x*pos.y;    )    
    NEXTBIT( i, bas[i] =   ir5p.y*pos.z;    )    
    NEXTBIT( i, bas[i] =   ir5p.z*pos.x;    )    
    return i;
}

/*
inline getMultipoleBasis( Vec3d pos, double * bas, uint32_t mask ){
    // https://en.wikipedia.org/wiki/Multipole_expansion#Expansion_in_Cartesian_coordinates
    double ir2 = 1/pos.norm2();
    double ir  = sqrt(r2);
    bas[0]     = ir;
    if(n<1)return;
    double ir3 = ir *ir2;
    bas[1]     = ir3*pos.z;
    bas[2]     = ir3*pos.y;
    bas[3]     = ir3*pos.x;
    if(n<4)return;
    // problem is they are linearily dependent => we have to use spherical 
    //double ir5 = ir3*ir2;
    Vec3d ir5p; ir3p.set_mul( pos, ir3*ir2 ); 
    bas[4 ]    = 2*ir5p.z*pos.z - ir5p.y*pos.y - ir5p.x*pos.x;
    bas[5 ]    =   ir5p.x*pos.x - ir5p.y*pos.y;
    bas[6 ]    = ir5p.x*pos.y;
    bas[7 ]    = ir5p.y*pos.z;
    bas[8 ]    = ir5p.z*pos.x;
    //bas[4 ]   += ir5p.x*pos.x;
    //bas[5 ]   += ir5p.x*pos.y;
    //bas[6 ]   += ir5p.x*pos.z; 
    //bas[7 ]   += ir5p.y*pos.x;
    //bas[8 ]   += ir5p.y*pos.y;
    //bas[9 ]   += ir5p.y*pos.z; 
    //bas[10]   += ir5p.z*pos.x;
    //bas[11]   += ir5p.z*pos.y;
    //bas[12]   += ir5p.z*pos.z; 
}
*/

void addOuterSymProduct( int n, double * B, double * BB ){
    int ii = 0;
    for(int i=0; i<n; i++){
        double bi = B[i];
        for(int j=i; j>=0; j--){
            BB[ii]+=bi*B[j];
            ii++;
        }
    }
}

// =====================================================
// ==========   Export these functions ( to Python )
// =====================================================

extern "C"{

// set pointer to force field array ( the array is usually allocated in python, we can flexibely switch betweeen different precomputed forcefields )
void setGrid_Pointer( double * data ){
	gridV = data;
}

// set forcefield grid dimension "n"
void setGridN( int * n ){
	//gridShape.n.set( *(Vec3i*)n );
	gridShape.n.set( n[2], n[1], n[0] );
	printf( " nxyz  %i %i %i \n", gridShape.n.x, gridShape.n.y, gridShape.n.z );
}

// set forcefield grid lattice vectors "cell"
void setGridCell( double * cell ){
	gridShape.setCell( *(Mat3d*)cell );
    gridShape.printCell();
}

/*
inline atom_sphere( rProbe  const Vec3d& pos,    bool &withinRmax, bool withinRmin  ){
	Vec3d dr;
	dr.set_sub( atom_pos[iatom], rProbe ); 
	double r2   = dr.norm2();
	double rmin = atom_Rmin[iatom];
	double rmax = atom_Rmax[iatom];
	if( r2<(rmax*rmax) ){ 
		if( atom_mask[iatom] ){ withinRmax = true; }  
		if( r2<(rmin*rmin)   ){ withinRmin = true; }
	}
}
*/

//  sampleGridArroundAtoms
//  	takes from a 3D grid values at grid points which are in area between $atom_Rmin[i] to $atom_Rmaxs[i] distance from any i-th atom at position $atom_posp[i]
//		if $canStore == true it will save values to $sampled_val and postions to $sampled_pos else it only returns number of gridpoints fullfilling the conditions
int sampleGridArroundAtoms( 
	int natoms, Vec3d * atom_pos_, double * atom_Rmin, double * atom_Rmax, bool * atom_mask, 
	double * sampled_val, Vec3d * sampled_pos_, bool canStore, bool pbc, bool show_where
){
	Vec3d * atom_pos    = (Vec3d*) atom_pos_;
	Vec3d * sampled_pos = (Vec3d*) sampled_pos_;
	int nx  = gridShape.n.x;
	int ny  = gridShape.n.y;
	int nz  = gridShape.n.z;
	int nxy = ny * nx; // used in macro i3D( ia, ib, ic )
	Vec3d rProbe;  rProbe.set( 0.0, 0.0, 0.0 ); // we may shift here
	int points_found = 0;
	int nimg = 1; if (pbc) nimg = 27; // PBC ?
	for ( int ia=0; ia<nx; ia++ ){ 
		//printf( " ia %i \n", ia );
		rProbe.add( gridShape.dCell.a );  
		for ( int ib=0; ib<ny; ib++ ){ 
			rProbe.add( gridShape.dCell.b );
			for ( int ic=0; ic<nz; ic++ ){
				rProbe.add( gridShape.dCell.c );
				bool withinRmin = false; 
				bool withinRmax = false; 
				for ( int iimg=0; iimg<nimg; iimg++ ){
					Vec3d cell_shift;
					cell_shift.set_lincomb( images[iimg][0], images[iimg][1], images[iimg][2], gridShape.cell.a, gridShape.cell.b, gridShape.cell.c );
					for ( int iatom=0; iatom<natoms; iatom++ ){
						Vec3d dr;
						dr.set_sub( rProbe, atom_pos[iatom] );
						dr.sub( cell_shift ); 
						double r2   = dr.norm2();
						double rmin = atom_Rmin[iatom];
						double rmax = atom_Rmax[iatom];
						if( r2<(rmax*rmax) ){ 
							if( atom_mask[iatom] ){ withinRmax = true; }  
							if( r2<(rmin*rmin)   ){ withinRmin = true; break; }
						}
					}
				}
				if( withinRmax && (!withinRmin) ){
					if( canStore ){
						sampled_val[points_found] = gridV[ i3D( ia, ib, ic ) ];
						if( show_where ) gridV[ i3D( ia, ib, ic ) ] = +100.0d;
						sampled_pos[points_found].set( rProbe );
					}
					points_found++;
				}
			} 
			rProbe.add_mul( gridShape.dCell.c, -nz );
		} 
		rProbe.add_mul( gridShape.dCell.b, -ny );
	}
	return points_found;
}

int buildLinearSystemMultipole(
    int npos,     double * poss_, double * V,
    int ncenter,  double * centers_, uint32_t * type, 
    double * bas, double * BB 
){    
    Vec3d* poss    = (Vec3d*)poss_;
    Vec3d* centers = (Vec3d*)centers_;
    int nbas; 
    for(int ipos=0; ipos<npos; ipos++){
        Vec3d pos = poss[ipos];
        int ibas = 0;
        for(int icenter=0; icenter<ncenter; icenter++ ){
            int nbi =  getMultipoleBasis( pos - centers[icenter], bas+ibas, type[icenter] );
            ibas+=nbi;
        }
        nbas=ibas;
        addOuterSymProduct( nbas, bas, BB );   // we need this since off-site multipoles are not orthogonal to each other
        for( int i=0; i<nbas; i++ ){ bas[i] *= V[ipos];  }
    }
    return nbas;
}

}





