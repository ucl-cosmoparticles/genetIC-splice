#include <cassert>
#include "fft.hpp"

using namespace std;

template<typename MyFloat>
struct grid_struct{

    long grid[3];
    long coords[3];
    MyFloat absval;
    MyFloat delta;

};

template<typename MyFloat>
class Grid{
private:
    std::shared_ptr<std::vector<std::complex<MyFloat>>> pField;
    bool fieldFourier; //< is the field in k-space (true) or x-space (false)?

public:

    const MyFloat dx,x0,y0,z0;
    const size_t size;
    const size_t size2;
    const size_t size3;

    std::vector<size_t> particleArray; // just a list of particles on this grid for one purpose or another

    std::vector<MyFloat*> particleProperties; // a list of particle properties



    Grid(size_t n,MyFloat dx=1.0, MyFloat x0=0.0, MyFloat y0=0.0, MyFloat z0=0.0) :
            dx(dx), x0(x0), y0(y0), z0(z0),
            size(n), size2(n*n), size3(n*n*n)
    {
        pField = std::make_shared<std::vector<std::complex<MyFloat>>>(size3,0);
        pField->shrink_to_fit();
    }

    std::vector<std::complex<MyFloat>> & get_field_fourier() {
        if(!fieldFourier) {
            fft(pField->data(),pField->data(),size,1);
            fieldFourier=true;
        }
        return *pField;
    }

    std::vector<std::complex<MyFloat>> & get_field_real() {
        if(fieldFourier) {
            fft(pField->data(),pField->data(),size,-1);
            fieldFourier=false;
        }
        return *pField;
    }

    std::vector<std::complex<MyFloat>> & get_field() {
        return *pField;
    }

    bool is_field_fourier() {
        return fieldFourier;
    }


    long find_next_ind(long index, const int step[3])
    {
        int grid[3];
        std::tie(grid[0],grid[1],grid[2])=get_coordinates(index);

        grid[0]+=step[0];
        grid[1]+=step[1];
        grid[2]+=step[2];

        return this->get_index(grid); // N.B. does wrapping inside get_index
    }

    long find_next_ind_no_wrap(long index, const int step[3])
    {

        int grid[3];
        std::tie(grid[0],grid[1],grid[2])=get_coordinates(index);

        grid[0]+=step[0];
        grid[1]+=step[1];
        grid[2]+=step[2];


        return this->get_index_no_wrap(grid);
    }

    void wrap(int &x, int &y, int &z)
    {
        x = x%size;
        y = y%size;
        z = z%size;
        if(x<0) x+=size;
        if(y<0) y+=size;
        if(z<0) z+=size;
    }

    void wrap(int pos[3])
    {
        wrap(pos[0],pos[1],pos[2]);
    }


    long get_index(int x, int y, int z)
    {

        long size=this->size;

        wrap(x,y,z);

        long index=(x*size+y);
        index*=size;
        index+=z;

        return index;

    }

    long get_index_no_wrap(int x, int y, int z)
    {

        long size=this->size;

        if(x<0 || x>=size || y<0 || y>=size || z<0 || z>=size)
            throw std::runtime_error("Grid index out of range in get_index_no_wrap");

        long index=(x*size+y);
        index*=size;
        index+=z;

        return index;

    }

    long get_index(const int pos[3]) {
        return get_index(pos[0], pos[1], pos[2]);
    }

    long get_index_no_wrap(const int pos[3]) {
        return get_index_no_wrap(pos[0], pos[1], pos[2]);
    }

    void get_coordinates(long id, int &x, int &y, int &z) {
        x = (int) (id/size2);
        y = (int) (id%size2)/size;
        z = (int) (id%size);

        // following check should be removed in the end:
        if(get_index(x,y,z)!=id) {
            cerr << "ERROR in get_coordinates";
            cerr << "id=" << id << " x,y,z=" << x << "," << y << "," << z << endl;
            cerr << "which gives " << get_index(x,y,z) << endl;
            assert(false);
        }
    }

    tuple<int, int, int> get_coordinates(long id) {
        int x, y, z;

        get_coordinates(id, x,y,z);

        return std::make_tuple(x,y,z);
    }

    void get_k_coordinates(long id, int &x, int &y, int &z) {
        get_coordinates(id,x,y,z);
        if(x>size/2) x=x-size;
        if(y>size/2) y=y-size;
        if(z>size/2) z=z-size;
    }

    tuple<int, int, int> get_k_coordinates(long id) {
        int x, y, z;

        get_k_coordinates(id, x,y,z);

        return std::make_tuple(x,y,z);
    }

    MyFloat get_abs_k_coordinates(long id) {
        int x,y,z;
        get_k_coordinates(id,x,y,z);
        return sqrt(x*x+y*y+z*z);
    }


    void get_centroid_location(long id, MyFloat &xc, MyFloat &yc, MyFloat &zc) {
        int x, y, z;
        get_coordinates(id,x,y,z);
        xc = x0+x*dx+dx/2;
        yc = y0+y*dx+dx/2;
        zc = z0+z*dx+dx/2;

    }
    tuple<MyFloat, MyFloat, MyFloat> get_centroid_location(long id) {
        MyFloat xc,yc,zc;
        get_centroid_location(id,xc,yc,zc);
        return std::make_tuple(xc,yc,zc);
    }




    vector<size_t> get_ids_in_cube(MyFloat x0c, MyFloat y0c, MyFloat z0c, MyFloat dxc) {
        // return all the grid IDs whose centres lie within the specified cube
        vector<size_t> ids;
        int xa=((int) floor((x0c-x0-dxc/2+dx/2)/dx));
        int ya=((int) floor((y0c-y0-dxc/2+dx/2)/dx));
        int za=((int) floor((z0c-z0-dxc/2+dx/2)/dx));

        int xb=((int) floor((x0c-x0+dxc/2-dx/2)/dx));
        int yb=((int) floor((y0c-y0+dxc/2-dx/2)/dx));
        int zb=((int) floor((z0c-z0+dxc/2-dx/2)/dx));

        for(int x=xa; x<=xb; x++) {
            for(int y=ya; y<=yb; y++) {
                for(int z=za; z<=zb; z++) {
                    ids.push_back(get_index(x,y,z));
                }
            }
        }

        return ids;

    }

    void add_grid(MyFloat *Pos1, MyFloat *Pos2, MyFloat *Pos3, MyFloat boxlen) {
        if(boxlen<0)
            boxlen = dx*size;

        MyFloat Mean1=0, Mean2=0, Mean3=0;
        size_t idx;

        for(int ix=0;ix<size;ix++) {
            for(int iy=0;iy<size;iy++) {
                for(int iz=0;iz<size;iz++) {

                    idx = static_cast<size_t>((ix*size+iy)*size+iz);

                    // position in physical coordinates
                    Pos1[idx]+= ix*dx+dx/2+x0;
                    Pos2[idx]+= iy*dx+dx/2+y0;
                    Pos3[idx]+= iz*dx+dx/2+z0;

                    // always wrap at the BASE level:
                    Pos1[idx] = fmod(Pos1[idx],boxlen);
                    if(Pos1[idx]<0) Pos1[idx]+=boxlen;
                    Pos2[idx] = fmod(Pos2[idx],boxlen);
                    if(Pos2[idx]<0) Pos2[idx]+=boxlen;
                    Pos3[idx] = fmod(Pos3[idx],boxlen);
                    if(Pos3[idx]<0) Pos3[idx]+=boxlen;


                    Mean1+=Pos1[idx];
                    Mean2+=Pos2[idx];
                    Mean3+=Pos3[idx];

                }
            }
        }


        cout<< "Box/2="<< boxlen/2.<< " Mpc/h, Mean position x,y,z: "<< Mean1/(MyFloat(size*size*size))<<" "<< Mean2/(MyFloat(size*size*size))<<" "<<Mean3/(MyFloat(size*size*size))<< " Mpc/h"<<  endl;

    }
};
