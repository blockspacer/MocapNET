/*
 * Utility to extract BVH files straight from OpenPose JSON output
 * Sample usage ./MocapNETJSON  --from frames/GOPR3223.MP4-data --label colorFrame_0_ --visualize
 * or           ./MocapNETJSON  --from 2dJoints_v1.2.csv --visualize
 */

#include "../MocapNETLib/mocapnet.hpp"
#include <iostream>
#include <vector>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "../MocapNETLib/tools.h"
#include "../MocapNETLib/jsonCocoSkeleton.h"
#include "../MocapNETLib/jsonMocapNETHelpers.hpp"
#include "../MocapNETLib/bvh.hpp"
#include "../MocapNETLib/visualization.hpp"

int main(int argc, char *argv[])
{
    const char   outputPathStatic[]="out.bvh";
    char * outputPath = (char*) outputPathStatic;
    
    unsigned int mocapNETMode=3;
    unsigned int width=1920 , height=1080 , frameLimit=10000 , visualize = 0, useCPUOnly=1 , serialLength=5,delay=0;
    unsigned int visWidth=1024,visHeight=768;
    const char * path=0;
    const char * label=0;
    float quality=1.0;

    unsigned int isJSONFile=1;
    unsigned int isCSVFile=0;

    if (initializeBVHConverter())
        {
            fprintf(stderr,"BVH code initalization successfull..\n");
        }


    for (int i=0; i<argc; i++)
        {
            if (strcmp(argv[i],"-v")==0)
                {
                    visualize=1;
                }
                else if (strcmp(argv[i],"--delay")==0)
                    {
                        //If you want to take some time to check the results that
                        //might otherwise pass by very fast
                        delay=atoi(argv[i+1]);
                    }
            else if (strcmp(argv[i],"--visualize")==0)
                {
                    visualize=1;
                }
            else if (strcmp(argv[i],"--maxFrames")==0)
                {
                    frameLimit=atoi(argv[i+1]);
                }
            else if (strcmp(argv[i],"--quality")==0)
                {
                    quality=atof(argv[i+1]);
                }
            else
                //if (strcmp(argv[i],"--cpu")==0)        { setenv("CUDA_VISIBLE_DEVICES", "", 1); } else
                if (strcmp(argv[i],"--gpu")==0)
                    {
                        useCPUOnly=0;
                    }
                else if (strcmp(argv[i],"--from")==0)
                    {
                        path = argv[i+1];
                        if (strstr(path,".json")!=0)
                            {
                                isJSONFile=1;
                                isCSVFile=0;
                            }
                        if (strstr(path,".csv")!=0)
                            {
                                isJSONFile=0;
                                isCSVFile=1;
                            }
                    }
                else if (strcmp(argv[i],"--label")==0)
                    {
                        label = argv[i+1];
                    }
                else if (strcmp(argv[i],"--seriallength")==0)
                    {
                        serialLength = atoi(argv[i+1]);
                    }
                else if  (  (strcmp(argv[i],"-o")==0) || (strcmp(argv[i],"--output")==0) )
                    {
                        outputPath=argv[i+1];
                    }                     
                else if (strcmp(argv[i],"--mode")==0)
                    { 
                        mocapNETMode=atoi(argv[i+1]);
                    }
                else if (strcmp(argv[i],"--size")==0)
                    {
                        width = atoi(argv[i+1]);
                        height = atoi(argv[i+2]);
                    }
        }

    if (path==0)
        {
            path="frames/dance.webm-data";
        }


    if (label==0)
        {
            label="colorFrame_0_";
        }


    struct CSVFileContext csv= {0};

    if (isCSVFile)
        {
            if (!openCSVFile(&csv,path))
                {
                    fprintf(stderr,"Unable to open CSV file %s \n",path);
                    return 0;
                }
        }


    struct MocapNET mnet= {0};
    if ( loadMocapNET(&mnet,"test",quality,mocapNETMode,useCPUOnly) )
        {
            char filePathOfJSONFile[1024]= {0};
            snprintf(filePathOfJSONFile,1024,"%s/colorFrame_0_00001.jpg",path);

            if ( getImageWidthHeight(filePathOfJSONFile,&width,&height) )
                {
                    fprintf(stderr,"Image dimensions changed from default to %ux%u\n",width,height);
                }
            else
                {
                    fprintf(stderr,"Assuming default image dimensions %ux%u , you can change this using --size x y\n",width,height);
                }


            std::vector<std::vector<float> >  empty2DPointsInput;

            float totalTime=0.0;
            unsigned int totalSamples=0;

            std::vector<std::vector<float> > bvhFrames;
            struct skeletonCOCO skeleton= {0};


            char formatString[256]= {0};
            snprintf(formatString,256,"%%s/%%s%%0%uu_keypoints.json",serialLength);


            unsigned int frameID=1;
            while (frameID<frameLimit)
                {
                    int okayToProceed=0;
                    snprintf(filePathOfJSONFile,1024,formatString,path,label,frameID);

                    if (isJSONFile)
                        {
                            if (parseJsonCOCOSkeleton(filePathOfJSONFile,&skeleton))
                                {
                                    okayToProceed=1;
                                }
                        }
                    else if (isCSVFile)
                        {
                            if ( parseNextCSVCOCOSkeleton(&csv,&skeleton) )
                                {
                                    okayToProceed=1;
                                }
                        }

                    if (okayToProceed)
                        {
                            unsigned int flatWidth=width,flatHeight=height;

                            if (isCSVFile)
                                {
                                    //CSV files come prenormalize so let's normalize them to 1x1 ( so leave them as they where )
                                    flatWidth=1;
                                    flatHeight=1;
                                }

                            std::vector<float> inputValues = flattenskeletonCOCOToVector(&skeleton,flatWidth,flatHeight);
                            if (inputValues.size()==0)
                                {
                                    fprintf(stderr,"Failed to read from JSON file..\n");
                                }

                            long startTime = GetTickCountMicrosecondsMN();
                            //--------------------------------------------------------
                            std::vector<float>  result = runMocapNET(&mnet,inputValues);
                            bvhFrames.push_back(result);
                            //--------------------------------------------------------
                            long endTime = GetTickCountMicrosecondsMN();


                            float sampleTime = (float) (endTime-startTime)/1000;
                            if (sampleTime==0.0)
                                {
                                    sampleTime=1.0;    //Take care of division by null..
                                }

                            float fpsMocapNET = (float) 1000/sampleTime;
                            fprintf(stderr,"Sample %u - %0.4fms - %0.4f fps\n",frameID,sampleTime,fpsMocapNET);


                            if (visualize)
                                {
                                    std::vector<std::vector<float> > points2DOutput = convertBVHFrameTo2DPoints(result,visWidth,visHeight);
                                    visualizePoints(
                                                     "3D Points Output",
                                                     frameID,
                                                     0,
                                                     0,
                                                     0,
                                                     0,
                                                     0,
                                                     0,
                                                     1,
                                                     1,
                                                     0.0,
                                                     0.0,
                                                     fpsMocapNET,
                                                     visWidth,
                                                     visHeight,
                                                     1,
                                                     0,0,0,//gesture stuff
                                                     inputValues,
                                                     result,
                                                     result,
                                                     empty2DPointsInput,
                                                     points2DOutput,
                                                     points2DOutput,
                                                     0 //No OpenGL code here..
                                                  );
                                }


                            totalTime+=sampleTime;
                            ++totalSamples;

                   if (delay!=0)
                   {
                       usleep(delay*1000);
                   }
                   
                        }
                    else
                        {
                            fprintf(stderr,"Done.. \n");
                            break;
                        }



                    ++frameID;
                }


            if (totalSamples>0)
                {
                    char * bvhHeaderToWrite=0;
                    if ( writeBVHFile(outputPath,bvhHeaderToWrite, bvhFrames) )
                        {
                            fprintf(stderr,"Successfully wrote %lu frames to bvh file %s.. \n",bvhFrames.size(),outputPath);
                        }
                    else
                        {
                            fprintf(stderr,"Failed to write %lu frames to bvh file %s .. \n",bvhFrames.size(),outputPath);
                        }



                    float averageTime=(float) totalTime/totalSamples;
                    fprintf(stderr,"\nTotal %0.2f ms for %u samples - Average %0.2f ms - %0.2f fps\n",totalTime,totalSamples,averageTime,(float) 1000/averageTime);
                }

            if (isCSVFile)
                {
                    closeCSVFile(&csv);
                }



            unloadMocapNET(&mnet);
        }
}
