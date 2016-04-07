/***************************************************************************
 *   Copyright (C) 2006 - 2016 by                                          *
 *      Tarek Taha, KURI  <tataha@tarektaha.com>                           *
 *      Randa Almadhoun   <randa.almadhoun@kustar.ac.ae>                   *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307, USA.          *
 ***************************************************************************/
#include "pathplanner.h"
namespace SSPP
{

PathPlanner::PathPlanner(ros::NodeHandle &n, Robot *rob, double conn_rad, int progressDisplayFrequency):
    nh(n),
    Astar(n,rob,progressDisplayFrequency),
    regGridConRadius(conn_rad),
    sampleOrientations(false)
{
}

PathPlanner::~PathPlanner()
{
    freeResources();
    std::cout<<"\n	--->>> Allocated Memory FREED <<<---"<<std::endl;
}

void PathPlanner::freeResources()
{
    freeSearchSpace();
    freePath();
    p=root=test=NULL;
}

void PathPlanner::freePath()
{
    while(path != NULL)
    {
        p = path->next;
        delete path;
        path = p;
    }
}

void PathPlanner::setConRad(double a)
{
    regGridConRadius = a;
}

void PathPlanner::generateRegularGrid(geometry_msgs::Pose gridStartPose,geometry_msgs::Vector3 gridSize, float gridRes, bool sampleOrientations)
{
    this->gridResolution = gridRes;
    this->sampleOrientations = sampleOrientations;
    generateRegularGrid(gridStartPose,gridSize);
}

void PathPlanner::generateRegularGrid(geometry_msgs::Pose gridStartPose,geometry_msgs::Vector3 gridSize, float gridRes)
{
    this->gridResolution = gridRes;
    generateRegularGrid(gridStartPose,gridSize);
}

void PathPlanner::generateRegularGrid(geometry_msgs::Pose gridStartPose, geometry_msgs::Vector3 gridSize)
{
    geometry_msgs::Pose pose;
    geometry_msgs::Pose correspondingSensorPose;
    int numSamples = 0;
    for(float x = gridStartPose.position.x; x<=(gridStartPose.position.x + gridSize.x); x+=gridResolution)
        for(float y = gridStartPose.position.y; y<=(gridStartPose.position.y + gridSize.y); y+=gridResolution)
            for(float z = gridStartPose.position.z; z<=(gridStartPose.position.z + gridSize.z); z+=gridResolution)
            {
                pose.position.z=z;
                pose.position.y=y;
                pose.position.x=x;
                //TODO:: add corresponding sensor transformation for CPP
                if(sampleOrientations)
                {
                    int orientationsNum= 360.0f/orientationResolution;
                    // in radians
                    double yaw=0.0;
                    tf::Quaternion tf ;
                    for(int i=0; i<orientationsNum;i++)
                    {
                        tf = tf::createQuaternionFromYaw(yaw);
                        pose.orientation.x = tf.getX();
                        pose.orientation.y = tf.getY();
                        pose.orientation.z = tf.getZ();
                        pose.orientation.w = tf.getW();
                        yaw+=(orientationResolution*M_PI/180.0f);
                        insertNode(pose,correspondingSensorPose);
                        numSamples++;
                    }
                }
                else
                {
                    pose.orientation.x=0;pose.orientation.y=0;pose.orientation.z=0;pose.orientation.w=1;
                    insertNode(pose,correspondingSensorPose);
                    numSamples++;
                }

            }
    std::cout<<"\n	--->>> REGULAR GRID GENERATED SUCCESSFULLY <<<--- Samples:"<<numSamples++;;
}

void PathPlanner::loadRegularGrid(const char *filename1, const char *filename2)
{
    geometry_msgs::Pose pose;
    geometry_msgs::Pose correspondingSensorPose;
    double locationx,locationy,locationz,qx,qy,qz,qw;
    double senLocx,senLocy,senLocz,senqx,senqy,senqz,senqw;

    assert(filename1 != NULL);
    filename1 = strdup(filename1);
    FILE *file1 = fopen(filename1, "r");

    assert(filename2 != NULL);
    filename2 = strdup(filename2);
    FILE *file2 = fopen(filename2, "r");
    if (!file1 || !file2)
    {
        std::cout<<"\nCan not open Search Space File";
        fclose(file1);
        fclose(file2);
    }
    while (!feof(file1) && !feof(file2))
    {
        fscanf(file1,"%lf %lf %lf %lf %lf %lf %lf\n",&locationx,&locationy,&locationz,&qx,&qy,&qz,&qw);
        fscanf(file2,"%lf %lf %lf %lf %lf %lf %lf\n",&senLocx,&senLocy,&senLocz,&senqx,&senqy,&senqz,&senqw);

        pose.position.x = locationx;
        pose.position.y = locationy;
        pose.position.z = locationz;
        pose.orientation.x = qx;
        pose.orientation.y = qy;
        pose.orientation.z = qz;
        pose.orientation.w = qw;

        correspondingSensorPose.position.x = senLocx;
        correspondingSensorPose.position.y = senLocy;
        correspondingSensorPose.position.z = senLocz;
        correspondingSensorPose.orientation.x = senqx;
        correspondingSensorPose.orientation.y = senqy;
        correspondingSensorPose.orientation.z = senqz;
        correspondingSensorPose.orientation.w = senqw;

        insertNode(pose,correspondingSensorPose);
    }
    fclose(file1);
    fclose(file2);
    std::cout<<"\n	--->>> REGULAR GRID GENERATED SUCCESSFULLY <<<---	";
}

std::vector<geometry_msgs::Point> PathPlanner::getSearchSpace()
{
    SearchSpaceNode *temp = searchspace;
    std::vector<geometry_msgs::Point> pts;
    while (temp != NULL)
    {
        geometry_msgs::Point pt;
        pt.x= temp->location.position.x;
        pt.y= temp->location.position.y;
        pt.z= temp->location.position.z;
        pts.push_back(pt);
        temp = temp->next;
    }
    return pts;
}

void PathPlanner::printNodeList()
{
    int step=1;
    geometry_msgs::Pose  pixel;
    if(!(p = this->path))
        return ;
    std::cout<<"\n--------------------   START OF LIST ----------------------";
    while(p !=NULL)
    {
        pixel.position.x =  p->pose.p.position.x;
        pixel.position.y =  p->pose.p.position.y;
        pixel.position.z =  p->pose.p.position.z;

        std::cout<<"\nStep [" << step++ <<"] x["<< pixel.position.x<<"] y["<<pixel.position.y<<"] z["<<pixel.position.z<< "]";
        std::cout<<"\tG cost="<<p->g_value<<"\tH cost="<<p->h_value<<"\tFcost="<<p->f_value;
        if (p->next !=NULL)
        {
            //cout<<"\tNext Angle = "<< setiosflags(ios::fixed) << setprecision(2)<<RTOD(atan2(p->next->location.y - p->location.y, p->next->location.x - p->location.x));
            //cout<<"\tAngle Diff ="<< setiosflags(ios::fixed) << setprecision(2)<<RTOD(anglediff(p->angle,atan2(p->next->location.y - p->location.y, p->next->location.x - p->location.x)));
        }
        p = p->next;
    }
    std::cout<<"\n--------------------   END OF LIST ---------------------- ";
}

void PathPlanner::connectNodes()
{
    SearchSpaceNode * S;
    SearchSpaceNode *temp;
    double distance;
    if (!searchspace)
        return;
    temp = searchspace;
    int numConnections=0;
    while (temp!=NULL)
    {
        S = searchspace;
        while (S!=NULL)
        {
            distance = Dist(S->location,temp->location);
            if (distance <= regGridConRadius && S != temp)// && distance !=0)
            {
                //check if parent and child are in the same position
                if (S->location.position.x != temp->location.position.x || S->location.position.y != temp->location.position.y || S->location.position.z != temp->location.position.z )
                {
                    if (heuristic->isConnectionConditionSatisfied(temp,S))
                    {
                        temp->children.push_back(S);
                        numConnections++;
                    }
                }
                //child and parent are in the same position.
                // TODO: check this logic
                else
                {
                    temp->children.push_back(S);
                }

            }
            S = S->next;
        }
        temp = temp->next;
    }
    std::cout<<"\n	--->>> NODES CONNECTED <<<---	";
    this->MAXNODES = numConnections;//searchspace->id;
}

std::vector<geometry_msgs::Point>  PathPlanner::getConnections()
{
    std::vector<geometry_msgs::Point> lineSegments;
    geometry_msgs::Point pt;
    SearchSpaceNode *temp = searchspace;
    while (temp != NULL)
    {
        for(int i=0; i < temp->children.size();i++)
        {
            pt.x = temp->location.position.x;
            pt.y = temp->location.position.y;
            pt.z = temp->location.position.z;
            lineSegments.push_back(pt);

            pt.x = temp->children[i]->location.position.x;
            pt.y = temp->children[i]->location.position.y;
            pt.z = temp->children[i]->location.position.z;
            lineSegments.push_back(pt);
        }
        temp = temp->next;
    }
    return lineSegments;
}

}