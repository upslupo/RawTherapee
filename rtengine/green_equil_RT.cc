////////////////////////////////////////////////////////////////
//
//			Green Equilibration via directional average
//
//	copyright (c) 2008-2010  Emil Martinec <ejmartin@uchicago.edu>
//
//
// code dated: February 12, 2011
//
//	green_equil_RT.cc is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////
#define TS 256	 // Tile size

#include <math.h>
#include <stdlib.h>
#include <time.h>


#define SQR(x) ((x)*(x))


//void green_equilibrate()//for dcraw implementation
void RawImageSource::green_equilibrate(float thresh)
{  
	// local variables
	static const int border=8;
	static const int border2=16;
	static const int v1=TS, v2=2*TS, v3=3*TS, /*v4=4*TS,*/ p1=-TS+1, p2=-2*TS+2, p3=-3*TS+3, m1=TS+1, m2=2*TS+2, m3=3*TS+3;
	
	int height=H, width=W; //for RT only
	int top, left; 

	int verbose=1;
	
	static const float eps=1.0;	//tolerance to avoid dividing by zero
	//static const float thresh=0.03;	//threshold for performing green equilibration; max percentage difference of G1 vs G2  
	// G1-G2 differences larger than this will be assumed to be Nyquist texture, and left untouched
	static const float diffthresh=0.25; //threshold for texture, not to be equilibrated

#pragma omp parallel
{		
	int top,left;
			char		*buffer;			// TS*TS*16
			float         (*cfa);		// TS*TS*4
			float         (*checker);			// TS*TS*4
			float         (*gvar);			// TS*TS*4
			float         (*gdiffv);			// TS*TS*4
			float         (*gdiffh);			// TS*TS*4
			
			/* assign working space */
			buffer = (char *) malloc(5*sizeof(float)*TS*TS);
			//merror(buffer,"green_equil()");
			//memset(buffer,0,5*sizeof(float)*TS*TS);
			
			cfa         = (float (*))		buffer;
			checker		= (float (*))			(buffer +	sizeof(float)*TS*TS);
			gvar		= (float (*))			(buffer +	2*sizeof(float)*TS*TS);
			gdiffv		= (float (*))			(buffer +	3*sizeof(float)*TS*TS);
			gdiffh		= (float (*))			(buffer +	4*sizeof(float)*TS*TS);
	
	
	

	
	// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	
	// Fill G interpolated values with border interpolation and input values
	// Main algorithm: Tile loop
#pragma omp for schedule(dynamic) nowait

	for (top=0; top < height-border; top += TS-border2)
		for (left=0; left < width-border; left += TS-border2) {
			int bottom = MIN( top+TS,height);
			int right  = MIN(left+TS, width);
			int numrows = bottom - top;
			int numcols = right - left;
			
			int row, col;
			int rr, cc, indx;
			int vote1, vote2;
			int counter, vtest;
						
			float gin, gse, gsw, gne, gnw, wtse, wtsw, wtne, wtnw;
			float mcorr, pcorr;
			float ginterp;
			
			float d1,d2,c1,c2;  
			float o1_1,o1_2,o1_3,o1_4;  
			float o2_1,o2_2,o2_3,o2_4;
			
			// rgb from input CFA data
			/* rgb values should be floating point number between 0 and 1 
			 after white balance multipliers are applied */
			for (rr=0; rr < numrows; rr++)
				for (row=rr+top, cc=0; cc < numcols; cc++) {
					col = cc+left;
					//cfa[rr*TS+cc] = image[row*width+col][FC(row,col)];//for dcraw implementation
					cfa[rr*TS+cc] = rawData[row][col];

				}
			
			//The green equilibration algorithm starts here
			
			for (rr=2; rr < numrows-2; rr++)
				for (cc=3-(FC(rr,2)&1), indx=rr*TS+cc; cc < numcols-2; cc+=2, indx+=2) {
					
					pcorr = (cfa[indx+p1]-cfa[indx])*(cfa[indx-p1]-cfa[indx]);
					mcorr = (cfa[indx+m1]-cfa[indx])*(cfa[indx-m1]-cfa[indx]);
					
					if (pcorr>0 && mcorr>0) {checker[indx]=1;} else {checker[indx]=0;}
					
					//checker[indx]=1;//test what happens if we always interpolate
				}
						
			counter=vtest=0;
			
			//now smooth the cfa data
			for (rr=6; rr < numrows-6; rr+=2)
				for (cc=7-(FC(rr,2)&1), indx=rr*TS+cc; cc < numcols-6; cc+=2, indx+=2) {
					if (checker[indx]) {
						//%%%%%%%%%%%%%%%%%%%%%%
						//neighbor checking code from Manuel Llorens Garcia
						o1_1=cfa[(rr-1)*TS+cc-1]; 
						o1_2=cfa[(rr-1)*TS+cc+1];  
						o1_3=cfa[(rr+1)*TS+cc-1]; 
						o1_4=cfa[(rr+1)*TS+cc+1];  
						o2_1=cfa[(rr-2)*TS+cc];  
						o2_2=cfa[(rr+2)*TS+cc];  
						o2_3=cfa[(rr)*TS+cc-2];  
						o2_4=cfa[(rr)*TS+cc+2];  
						
						d1=(o1_1+o1_2+o1_3+o1_4)/4.0;  
						d2=(o2_1+o2_2+o2_3+o2_4)/4.0;  
						
						c1=(fabs(o1_1-o1_2)+fabs(o1_1-o1_3)+fabs(o1_1-o1_4)+fabs(o1_2-o1_3)+fabs(o1_3-o1_4)+fabs(o1_2-o1_4))/6.0;  
						c2=(fabs(o2_1-o2_2)+fabs(o2_1-o2_3)+fabs(o2_1-o2_4)+fabs(o2_2-o2_3)+fabs(o2_3-o2_4)+fabs(o2_2-o2_4))/6.0;
						//%%%%%%%%%%%%%%%%%%%%%%
						
						vote1=(checker[indx-v2]+checker[indx-2]+checker[indx+2]+checker[indx+v2]);
						vote2=(checker[indx-m1]+checker[indx+p1]+checker[indx-p1]+checker[indx+m1]);
						//if ((vote1==0 || vote2==0) && (c1+c2)<2*thresh*fabs(d1-d2)) vtest++;
						if (vote1>0 && vote2>0 && (c1+c2)<4*thresh*fabs(d1-d2)) {
							//pixel interpolation
							gin=cfa[indx];
							
							gse=(cfa[indx+m1])+0.5*(cfa[indx]-cfa[indx+m2]);
							gnw=(cfa[indx-m1])+0.5*(cfa[indx]-cfa[indx-m2]);
							gne=(cfa[indx+p1])+0.5*(cfa[indx]-cfa[indx+p2]);
							gsw=(cfa[indx-p1])+0.5*(cfa[indx]-cfa[indx-p2]);
							
							
							
							wtse=1/(eps+SQR(cfa[indx+m2]-cfa[indx])+SQR(cfa[indx+m3]-cfa[indx+m1]));
							wtnw=1/(eps+SQR(cfa[indx-m2]-cfa[indx])+SQR(cfa[indx-m3]-cfa[indx-m1]));
							wtne=1/(eps+SQR(cfa[indx+p2]-cfa[indx])+SQR(cfa[indx+p3]-cfa[indx+p1]));
							wtsw=1/(eps+SQR(cfa[indx-p2]-cfa[indx])+SQR(cfa[indx-p3]-cfa[indx-p1]));
							
							ginterp=(gse*wtse+gnw*wtnw+gne*wtne+gsw*wtsw)/(wtse+wtnw+wtne+wtsw);
							
							//if ( ((ginterp-gin) < thresh*(ginterp+gin)) ) {
								cfa[indx]=0.5*(ginterp+gin);
								counter++;
							//}
							
						}
					}
				}
			//printf("pixfix count= %d; vtest= %d \n",counter,vtest);
			// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
			
			// copy smoothed results back to image matrix
			for (rr=border; rr < numrows-border; rr+=2)
				for (row=rr+top, cc=border+1-(FC(rr,2)&1), indx=rr*TS+cc; cc < numcols-border; cc+=2, indx+=2) {
					col = cc + left;
					//c = FC(row,col);
					//image[row*width + col][c] = CLIP((int)(cfa[indx] + 0.5)); //for dcraw implementation
					rawData[row][col] = cfa[indx];
				} 

			// clean up
			}
			free(buffer);
		
		}

	
	// done
	/*t2 = clock();
	dt = ((double)(t2-t1)) / CLOCKS_PER_SEC;
	if (verbose) {
		fprintf(stderr,_("elapsed time = %5.3fs\n"),dt);
	}*/
	
	
}
#undef TS
