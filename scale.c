/*
	scale attributes to [lower,upper]
	usage: scale [-l lower] [-u upper] filename
*/
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_LINE_LEN 100000

char buf[MAX_LINE_LEN];
double lower=-1.0,upper=1.0;
double *feature_max;
double *feature_min;
int max_index;

#define max(x,y) ((x>y)?x:y)
#define min(x,y) ((x<y)?x:y)

void output(int index, double value);

int main(int argc,char **argv)
{
	int i,index;
	FILE *fp;

	while(1)
	{
		int c = getopt(argc,argv,"l:u:");
		if(c == EOF) break;
		switch(c)
		{
			case 'l': lower = atof(optarg); break;
			case 'u': upper = atof(optarg); break;
			case '?':
				exit(1);
		}
	}
	
	if(!(upper > lower))
	{
		fprintf(stderr,"inconsistent lower/upper specification\n");
		exit(1);
	}
	
	if(argc != optind+1) 
	{
		fprintf(stderr,"usage: %s [-l lower] [-u upper] filename\n",argv[0]);
		exit(1);
	}

	fp=fopen(argv[optind],"r");
	
	if(fp==NULL)
	{
		fprintf(stderr,"can't open file\n");
		exit(1);
	}

#define SKIP_CLASS\
	while(isspace(*p)) ++p;\
	while(!isspace(*p)) ++p;

#define SKIP_ELEMENT\
	while(*p!=':') ++p;\
	++p;\
	while(isspace(*p)) ++p;\
	while(*p && !isspace(*p)) ++p;
	
	/* assumption: min index of attributes is 1 */
	/* pass 1: find out max index of attributes */
	max_index = 0;
	while(fgets(buf,MAX_LINE_LEN,fp)!=NULL)
	{
		char *p=buf;

		SKIP_CLASS

		while(sscanf(p,"%d:%*f",&index)==1)
		{
			max_index = max(max_index, index);
			SKIP_ELEMENT
		}		
	}
	
	feature_max = (double *)malloc((max_index+1)* sizeof(double));
	feature_min = (double *)malloc((max_index+1)* sizeof(double));
	
	if(feature_max == NULL || feature_min == NULL)
	{
		fprintf(stderr,"can't allocate enough memory\n");
		exit(1);
	}

	for(i=0;i<=max_index;i++)
	{
		feature_max[i]=-DBL_MAX;
		feature_min[i]=DBL_MAX;
	}

	rewind(fp);

	/* pass 2: find out min/max value */
	while(fgets(buf,MAX_LINE_LEN,fp)!=NULL)
	{
		char *p=buf;
		int next_index=1;
		double value;
		
		SKIP_CLASS

		while(sscanf(p,"%d:%lf",&index,&value)==2)
		{
			for(i=next_index;i<index;i++)
			{
				feature_max[i]=max(feature_max[i],0);
				feature_min[i]=min(feature_min[i],0);
			}
			
			feature_max[index]=max(feature_max[index],value);
			feature_min[index]=min(feature_min[index],value);

			SKIP_ELEMENT
			next_index=index+1;
		}		

		for(i=next_index;i<=max_index;i++)
		{
			feature_max[i]=max(feature_max[i],0);
			feature_min[i]=min(feature_min[i],0);
		}	
	}

	rewind(fp);

	/* pass 3: scale */
	while(fgets(buf,MAX_LINE_LEN,fp)!=NULL)
	{
		char *p=buf;
		int next_index=1;
		int index;
		int class;
		double value;
		
		sscanf(p,"%d",&class);
		printf("%+d ",class);

		SKIP_CLASS

		while(sscanf(p,"%d:%lf",&index,&value)==2)
		{
			for(i=next_index;i<index;i++)
				output(i,0);
			
			output(index,value);

			SKIP_ELEMENT
			next_index=index+1;
		}		

		for(i=next_index;i<=max_index;i++)
			output(i,0);

		printf("\n");
	}

	fclose(fp);
	return 0;
}

void output(int index, double value)
{
	/* skip single-valued attribute */
	if(feature_max[index] == feature_min[index])
		return;

	if(value == feature_min[index])
		value = lower;
	else if(value == feature_max[index])
		value = upper;
	else
		value = lower + (upper-lower) * 
			(value-feature_min[index])/
			(feature_max[index]-feature_min[index]);

	if(value != 0)
		printf("%d:%g ",index, value);
}
