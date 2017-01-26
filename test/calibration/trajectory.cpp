/*
This file is part of visgeom.

visgeom is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

visgeom is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with visgeom.  If not, see <http://www.gnu.org/licenses/>.
*/ 

#include "io.h"
#include "ocv.h"
#include "eigen.h"

#include "geometry/geometry.h"
#include "calibration/trajectory_generation.h"

class CircularTrajectory : public ITrajectory
{
public:
    CircularTrajectory(int numberSteps, int numberCircles = 1) : 
        _numberSteps(numberSteps),
        _numberCircles(numberCircles) {}
        
    virtual void compute(const double * params, vector<Transf> & trajVec, 
            vector<Matrix6d> & covVec) const
    {
        trajVec.clear();
        
        //control covariance
        Matrix2d covVW;
        //TODO make parameters
        
        Matrix6d covAbs = Matrix6d::Identity()*1e-3;
        for (int circle = 0; circle < _numberCircles; circle++)
        {
            double alpha = params[2*circle]; //turn angle
            double dist = params[2*circle + 1];
            double ca = cos(alpha*0.5);
            double sa = sin(alpha*0.5);
            
            //TODO modelizer proprement l'odometrie            
            covVW <<    1e-4,        0, 
                        0,       1e-4;
                    
            //circular motion model
            Transf dxi(dist * ca, dist * sa, 0, 0, 0, alpha);
            //motion jacobian
            Matrixd<6, 2> dxidu;
            dxidu <<    ca,     -dist/2 * sa,
                        sa,     dist/2 * ca,
                        0,          0,
                        0,          0,
                        0,          0,
                        0,          1;
            Matrix6d covIncr = dxidu * covVW * dxidu.transpose();
            trajVec.push_back(dxi);
            covVec.push_back(covIncr + covAbs);
            
            //Screw transformation matrix
            Matrix3d R = dxi.rotMatInv();
            Matrix6d L; //TODO make a separate function
            L.topLeftCorner<3, 3>() = R;
            L.topRightCorner<3, 3>() = -R * hat(dxi.trans());
            L.bottomLeftCorner<3, 3>() = Matrix3d::Zero();
            L.bottomRightCorner<3, 3>() = R;
            Matrix6d covOdom = covIncr;
            for (int i = 1; i < _numberSteps; i++)
            {
                trajVec.push_back(trajVec.back().compose(dxi));
                covOdom = L * covOdom * L.transpose() + covIncr;
                covVec.push_back(covOdom + covAbs);
            }
        }
    }
    
    virtual int paramSize() const { return _numberCircles * 2; }
    
    int _numberSteps;
    int _numberCircles;
};


int main(int argc, char** argv) 
{
    int circleCount = 1;
    vector<double> paramVec;
    for (int i = 0; i < circleCount; i++)
    {
        paramVec.push_back(i * 0.01 + 0.016);
        paramVec.push_back(0.01);
    }
    Transf xiCam(0.2, 0, 0.3, 1.2, 1.2, 1.2);
    double numberSteps = 100;
    CircularTrajectory * traj = new CircularTrajectory(numberSteps, circleCount);
    vector<Transf> xiOdomVec;
    vector<Matrix6d> covOdomVec;
//    traj->compute(paramVec.data(), xiOdomVec, covOdomVec);
//    
//    for (int i = 0; i < xiOdomVec.size(); i++)
//    {
//        cout << xiOdomVec[i] << endl;
//        cout << covOdomVec[i] << endl << endl;
//    }
    
    TrajectoryQuality * costFunction = new TrajectoryQuality(
                                            traj,
                                            xiCam,
                                            Matrix6d::Identity()*1e-3,
                                            Matrix6d::Identity());
                                            
    
    //Improvised gradient descend 
    vector<double> gradVec(paramVec.size());
    vector<double> paramVec2 = paramVec;
    double val, val2 = 999999;
    double lambda = 1e-2;
    for (int iter = 0; iter < 10000; iter++)
    {
        costFunction->Evaluate(paramVec.data(), &val, gradVec.data());
        cout << iter << " " << val <<  " " << lambda <<endl;
        double gradNorm = 0;
        for (int i = 0; i < paramVec.size(); i++)
        {
            gradNorm += gradVec[i]*gradVec[i];
        }
        if (val > val2)
        {
            lambda = max(1e-8, lambda / 3); 
            paramVec = paramVec2;
            continue;
        }
        
        val2 = val;
        paramVec2 = paramVec;
        
        if (gradNorm > lambda*lambda)
        {
            gradNorm = sqrt(gradNorm);
            for (int i = 0; i < paramVec.size(); i++)
            {
                gradVec[i] *= lambda / gradNorm;
            }
        }
        for (int i = 0; i < paramVec.size(); i++)
        {
            paramVec[i] -= gradVec[i] * lambda;
        }
    }

    for (int i = 0; i < circleCount; i++)
    {
        cout << paramVec[i * 2] << "   " << paramVec[i * 2 + 1] << endl;
    }
    
    traj->compute(paramVec.data(), xiOdomVec, covOdomVec);
//    
//    for (int i = 0; i < xiOdomVec.size(); i++)
//    {
//        cout << xiOdomVec[i] << endl;
//        cout << covOdomVec[i].diagonal().transpose() << endl << endl;
//    }
    
}
