import java.io.*;
import java.util.*;

class svm_classify {
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
			double decision_value = svm.svm_classify(model,x);
			output.writeBytes(decision_value+"\n");
			if(decision_value*label > 0)
				++correct;
			++total;
		}
		System.out.print("correct/total = "+correct+"/"+total
				+" ("+(double)correct/total*100+"%)\n");
	}

	public static void main(String argv[]) throws IOException
	{
		if(argv.length != 3)
		{
			System.err.print("usage: svm-classify test_file model_file output_file\n");
			System.exit(1);
		}

		BufferedReader input = new BufferedReader(new FileReader(argv[0]));
		DataOutputStream output = new DataOutputStream(new FileOutputStream(argv[2]));
		svm_model model = svm.svm_load_model(argv[1]);
		classify(input,output,model);
	}
}
