import java.io.*;
import java.util.*;

class svm_predict {
	private static double atof(String s)
	{
		return Double.valueOf(s).doubleValue();
	}

	private static int atoi(String s)
	{
		return Integer.parseInt(s);
	}

	private static void classify(BufferedReader input, DataOutputStream output, svm_model model) throws IOException
	{
		int correct = 0;
		int total = 0;
		double error = 0;

		while(true)
		{
			String line = input.readLine();
			if(line == null) break;

			StringTokenizer st = new StringTokenizer(line," \t\n\r\f:");

			double label = atof(st.nextToken());
			int m = st.countTokens()/2;
			svm_node[] x = new svm_node[m];
			for(int j=0;j<m;j++)
			{
				x[j] = new svm_node();
				x[j].index = atoi(st.nextToken());
				x[j].value = atof(st.nextToken());
			}
			double v = svm.svm_predict(model,x);
			if(v == label)
				++correct;
			error += (v-label)*(v-label);
			++total;

			output.writeBytes(v+"\n");
		}
		System.out.print("Accuracy = "+(double)correct/total*100+
				 "% ("+correct+"/"+total+") (classification)\n");
		System.out.print("Mean squared error = "+error/total+" (regression)\n");
	}

	public static void main(String argv[]) throws IOException
	{
		if(argv.length != 3)
		{
			System.err.print("usage: svm-predict test_file model_file output_file\n");
			System.exit(1);
		}

		BufferedReader input = new BufferedReader(new FileReader(argv[0]));
		DataOutputStream output = new DataOutputStream(new FileOutputStream(argv[2]));
		svm_model model = svm.svm_load_model(argv[1]);
		classify(input,output,model);
	}
}
