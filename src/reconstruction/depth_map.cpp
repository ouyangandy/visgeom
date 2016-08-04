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

/*
Depth container. 
NOTE:
(u, v) is an image point 
(x, y) is a depth map point
*/

#include "reconstruction/depth_map.h"

#include "io.h"
#include "std.h"
#include "eigen.h"

void DepthMap::applyMask(const Mat8u & mask)
{
    for (int y = 0; y < yMax; y++)
    {
        for (int x = 0; x < xMax; x++)
        {
            if ( not mask(v(y), u(x)) ) 
            {
                for (int h = 0; h < hMax; h++)
                {
                    at(x, y, h) = 0;
                }
            }
        }
    }
}

//check the limits
bool DepthMap::isValid(const int x, const int y, const int h) const
{
    return ( (x >= 0) and (x < xMax) and (y >= 0) and (y < yMax) and (h >= 0) and (h < hMax) );
}


// nearest neighbor interpolation
double DepthMap::nearest(const int u, const int v, const int h) const
{
    int xd = x(u);
    int yd = y(v);
    if (isValid(xd, yd, h)) return at(xd, yd, h);
    else return OUT_OF_RANGE;
}

// nearest neighbor interpolation
double DepthMap::nearest(const Vector2d pt, const int h) const
{
    int xd = x(pt[0]);
    int yd = y(pt[1]);
    if (isValid(xd, yd, h)) return at(xd, yd, h);
    else return OUT_OF_RANGE;
}


// nearest neighbor interpolation
double DepthMap::nearestSigma(const int u, const int v, const int h) const
{
    int xd = x(u);
    int yd = y(v);
    if (isValid(xd, yd, h)) return sigma(xd, yd, h);
    else return OUT_OF_RANGE;
}

// nearest neighbor interpolation
double DepthMap::nearestSigma(const Vector2d pt, const int h) const
{
    int xd = x(pt[0]);
    int yd = y(pt[1]);
    if (isValid(xd, yd, h)) return sigma(xd, yd, h);
    else return OUT_OF_RANGE;
}

// nearest neighbor interpolation
double DepthMap::nearestCost(const int u, const int v, const int h) const
{
    int xd = x(u);
    int yd = y(v);
    if (isValid(xd, yd, h)) return cost(xd, yd, h);
    else return OUT_OF_RANGE;
}

// nearest neighbor interpolation
double DepthMap::nearestCost(const Vector2d pt, const int h) const
{
    int xd = x(pt[0]);
    int yd = y(pt[1]);
    if (isValid(xd, yd, h)) return cost(xd, yd, h);
    else return OUT_OF_RANGE;
}


// to access the elements directly
double & DepthMap::at(const int x, const int y, const int h)
{
    return valVec[x + y*xMax + h*hStep];
}
const double & DepthMap::at(const int x, const int y, const int h) const
{
    return valVec[x + y*xMax + h*hStep];
}

// to access the elements directly
double & DepthMap::at(const int idx)
{
    return valVec[idx];
}
const double & DepthMap::at(const int idx) const
{
    return valVec[idx];
}

// to access the uncertainty directly
double & DepthMap::sigma(const int x, const int y, const int h)
{
    return sigmaVec[x + y*xMax + h*hStep];
}
const double & DepthMap::sigma(const int x, const int y, const int h) const
{
    return sigmaVec[x + y*xMax + h*hStep];
}

// to access the uncertainty directly
double & DepthMap::sigma(const int idx)
{
    return sigmaVec[idx];
}
const double & DepthMap::sigma(const int idx) const
{
    return sigmaVec[idx];
}

// to access the hypothesis cost directly
double & DepthMap::cost(const int x, const int y, const int h)
{
    return costVec[x + y*xMax + h*hStep];
}
const double & DepthMap::cost(const int x, const int y, const int h) const
{
    return costVec[x + y*xMax + h*hStep];
}

// to access the hypothesis cost directly
double & DepthMap::cost(const int idx)
{
    return costVec[idx];
}
const double & DepthMap::cost(const int idx) const
{
    return costVec[idx];
}

Vector2dVec DepthMap::getPointVec(const std::vector<int> idxVec) const
{
    Vector2dVec result;
    result.reserve(idxVec.size());
    for (auto & idx : idxVec)
    {
        const int idxh = idx % hStep;
        result.emplace_back(u(idxh % xMax), v(idxh / xMax));
    }
    return result;
}

Vector2dVec DepthMap::getPointVec() const
{
    Vector2dVec result;
    result.reserve(xMax * yMax);
    for (int h = 0; h < hMax; h++)
    {
        for (int y = 0; y < yMax; y++)
        {
            for (int x = 0; x < xMax; x++)
            {
                result.emplace_back(u(x), v(y)); 
            }
        }
    }
    return result;
}

void DepthMap::reconstructUncertainty(vector<int> & idxVec, 
            Vector3dVec & minDistVec, Vector3dVec & maxDistVec) const
{
    minDistVec.clear();
    maxDistVec.clear();
    idxVec.clear();
    vector<double> minVec;
    vector<double> maxVec;
    vector<int> idxBrutVec;
    for (int i = 0; i < valVec.size(); i++)
    {
        double d = valVec[i];
        if (d >= MIN_DEPTH)
        {
            double s = sigmaVec[i];
            // take d +- 2*sigma
            minVec.push_back(max(MIN_DEPTH, d - 2*s));
            maxVec.push_back(d + 2*s);
            idxBrutVec.push_back(i);
        }
    }
    
    Vector2dVec pointBrutVec = getPointVec(idxBrutVec);
    
    Vector3dVec reconstBrutVec;
    vector<bool> maskVec;
    cameraPtr->reconstructPointCloud(pointBrutVec, reconstBrutVec, maskVec);
    
    for (int i = 0; i < reconstBrutVec.size(); i++)
    {
        if (maskVec[i])
        {
            Vector3d X = reconstBrutVec[i].normalized();
            minDistVec.push_back(X*minVec[i]);
            maxDistVec.push_back(X*maxVec[i]);
            idxVec.push_back(idxBrutVec[i]);        
        }
    }
}

void DepthMap::reconstruct(MHPack & result, const Flags flags) const
{
    result.idxVec.clear();
    result.hypIdxVec.clear();
    result.costVec.clear();
    result.cloud.clear();
    result.valVec.clear();

    vector<double> depthVec;

    const bool storeSigma = (bool)(flags & RECONSTRUCTION_WITH_SIGMA);
    const bool minmax = (bool)(flags & MINMAX_DISTANCE_VEC_WITH_EMPTY);

    if ( not (flags & RECONSTRUCT_QUERY_POINTS))
    {
        result.imagePointVec.clear();

        const int searchsize = ( flags & ADD_ALL_HYPOTHESES ) ? valVec.size() : hStep; // Reminder: valVec.size() == hMax*hStep
        for (int i = 0; i < searchsize; i++)
        {
            const double d = valVec[i];
            if (d >= MIN_DEPTH)
            {
                depthVec.push_back(d);
                result.idxVec.push_back( i % hStep );
                result.hypIdxVec.push_back( i / hStep );
                result.costVec.push_back( costVec[i] );
                if (storeSigma) result.valVec.push_back( sigmaVec[i] );
            }
        }
        result.imagePointVec = getPointVec(result.idxVec);
    }
    else
    {
        const Vector2dVec queryPointVec = result.imagePointVec;
        for (int i = 0; i < queryPointVec.size(); i++)
        {
            const int hypSize = (flags & ADD_ALL_HYPOTHESES) ? hMax : 1;
            const Vector2d & imagePoint = queryPointVec[i];
            for(int h = 0; h < hypSize; h++)
            {
                const double d = nearest(imagePoint, h);
                if (d >= MIN_DEPTH)
                {
                    depthVec.push_back( d );
                    result.idxVec.push_back( x(imagePoint[0]) + y(imagePoint[1])*xMax );
                    result.hypIdxVec.push_back( h );
                    result.costVec.push_back( nearestCost(imagePoint, h) );
                    if (storeSigma) result.valVec.push_back( nearestSigma(imagePoint, h) );
                }
            }
        }
        result.imagePointVec = getPointVec(result.idxVec);
    }

    vector<bool> maskVec;
    cameraPtr->reconstructPointCloud(result.imagePointVec, result.cloud, maskVec);

    for (int i = 0; i < result.cloud.size(); i++)
    {
        if (maskVec[i])
        {
            result.cloud[i] = result.cloud[i].normalized() * depthVec[i];
            if (not storeSigma) result.valVec[i] = 0; // Access the image value and store here
        }
        else //TODO - Need to fix this
        {
            result.cloud[i] << 0,0,0; // Insert null vector
            if (not storeSigma) result.valVec[i] = 0;
        }
    }
}

//TODO - Remove this and the next reconstruct() after updating the code
void DepthMap::reconstruct(vector<int> & idxVec, Vector3dVec & result) const
{
    result.clear();
    idxVec.clear();
    vector<double> depthVec;
    vector<int> idxBrutVec;
    for (int i = 0; i < valVec.size(); i++)
    {
        double d = valVec[i];
        if (d >= MIN_DEPTH)
        {
            depthVec.push_back(d);
            idxBrutVec.push_back(i);
        }
    }
    Vector2dVec pointBrutVec = getPointVec(idxBrutVec);
    
    Vector3dVec reconstBrutVec;
    vector<bool> maskVec;
    cameraPtr->reconstructPointCloud(pointBrutVec, reconstBrutVec, maskVec);
    
    for (int i = 0; i < reconstBrutVec.size(); i++)
    {
        if (maskVec[i])
        {
            Vector3d X = reconstBrutVec[i].normalized();
            result.push_back(X*depthVec[i]);
            idxVec.push_back(idxBrutVec[i]);        
        }
    }
}

void DepthMap::reconstruct(const Vector2dVec & queryPointVec,
        vector<int> & idxVec, Vector3dVec & result) const
{
    result.clear();
    idxVec.clear();
    Vector3dVec reconstBrutVec;
    vector<bool> maskVec;
    cameraPtr->reconstructPointCloud(queryPointVec, reconstBrutVec, maskVec);
    for (int i = 0; i < queryPointVec.size(); i++)
    {
        if (maskVec[i])
        {
            double d = nearest(queryPointVec[i]);
            if (d < MIN_DEPTH) continue;
            Vector3d X = reconstBrutVec[i].normalized();
            result.push_back(X*d);
            idxVec.push_back(i);        
        }
    }
}


void DepthMap::project(const Vector3dVec & pointVec, Vector2dVec & result) const
{
    cameraPtr->projectPointCloud(pointVec, result);
}

void DepthMap::toMat(Mat32f & out) const
{
    out.create(yMax, xMax);
    copy(valVec.begin(), valVec.begin()+hStep, (float*)out.data);
}

//TODO do not reconstruct all the points but a selected subset
// to avoid reconstruction of points with bad disparity
//TODO Anoop - Rewrite this later
void DepthReprojector::wrapDepth(const DepthMap& dMap1, const DepthMap& dMap2,
        const Transformation<double> T12, DepthMap& output)
{
	//Step 1 : Get point-cloud of first camera in first frame
	vector<int> idx0Vec;
	Vector3dVec cloud11;
	dMap1.reconstruct(idx0Vec, cloud11);

	//Step 2 : Transform above into second frame
	Vector3dVec cloud12;
	T12.inverseTransform(cloud11, cloud12);

	//Step 3 : Reproject points into second camera
	Vector2dVec point12Vec;
	dMap2.project(cloud12, point12Vec);

	//Step 4 : For reprojected points, reconstruct point-cloud of second camera in second frame
	Vector3dVec cloud22;
	vector<int> idx1Vec;
	dMap2.reconstruct(point12Vec, idx1Vec, cloud22);

	//Step 5 : Transform above into first frame
	Vector3dVec cloud21;
	T12.transform(cloud22, cloud21);

	//Step 6 : Project above points along corresponding depth vectors
    output = dMap1;
    output.setTo(0, 1);
	for(int i = 0; i < idx1Vec.size(); ++i)
	{
	    int idx1 = idx1Vec[i];
	    int idx0 = idx0Vec[idx1];
		const Vector3d & X2 = cloud21[i];
		const Vector3d & X1 = cloud11[idx1];
		//Calculate dot-product to get the distance as the projection along the line
		output.at(idx0) =  X2.dot(X1.normalized());
		output.sigma(idx0) = dMap2.nearestSigma(point12Vec[idx1]);
	}
}

DepthMap DepthMap::generatePlane(const ICamera * camera, const ScaleParameters & params, 
        Transformation<double> TcameraPlane, const Vector3dVec & polygonVec)
{
    DepthMap depth(camera, params);
    Vector3d t = TcameraPlane.trans();
    Vector3d z = TcameraPlane.rotMat().col(2);
    Vector3dVec polygonCamVec;
    TcameraPlane.transform(polygonVec, polygonCamVec);
    for (int v = 0; v < params.yMax; v++)
    {
        for (int u = 0; u < params.xMax; u++)
        {
            depth.at(u, v) = 0;
            Vector3d vec; // the direction vector
            if (not camera->reconstructPoint(Vector2d(params.u(u), params.v(v)), vec)) continue;
            double zvec = z.dot(vec);
            if (zvec < 1e-3) 
            {
                continue;
            }
            bool inside = true;
            for (int i = 0; i < polygonCamVec.size(); i++)
            {
                int j = (i + 1) % polygonCamVec.size();
                Vector3d normal = polygonCamVec[i].cross(polygonCamVec[j]);
                if (vec.dot(normal) < 0)
                {
                    inside = false;
                    break;
                }
            }
            if (not inside) continue;
            double tz = t.dot(z);
            double alpha = tz / zvec;
            vec *= alpha;
            depth.at(u, v) = vec.norm();
        }
    }
    return depth;
}

