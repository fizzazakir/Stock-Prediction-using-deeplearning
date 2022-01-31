// Header files
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

using namespace std;

//----------------------------------------------------------------------------
// Calculates volatility from data.csv file
//----------------------------------------------------------------------------
float calculate_volatility(float spot_price, int time_steps)
{
    // Open data.csv in read-mode, exit on fail
    const char* file_name = "data.csv";
    ifstream file_ptr;
    file_ptr.open(file_name, ifstream::in);
    if (!file_ptr.is_open())
    {
        cerr << "Cannot open data.csv! Exiting..\n";
        exit(EXIT_FAILURE);
    }

    string line;
    // Read the first line then close file
    if (!getline(file_ptr, line))
    {
        cerr << "Cannot read from data.csv! Exiting..\n";
        file_ptr.close();
        exit(EXIT_FAILURE);
    }
    file_ptr.close();

    int i = 0, len = time_steps - 1;
    unique_ptr<float[]> priceArr = make_unique<float[]>(time_steps - 1);

    istringstream iss(line);
    string token;

    // Get the return values of stock from file (min 2 to 180)
    while (getline(iss, token, ',')){
        priceArr[i++] = stof(token);
    }

    float sum = spot_price;
    // Find mean of the estimated minute-end prices
    for (i = 0; i < len; i++){
        sum += priceArr[i];
    }
    float mean_price = sum / (len + 1);

    // Calculate market volatility as standard deviation
    sum = powf((spot_price - mean_price), 2.0f);
    for (i = 0; i < len; i++)
        sum += powf((priceArr[i] - mean_price), 2.0f);

    float std_dev = sqrtf(sum);

    // Return as percentage
    return std_dev / 100.0f;
}

/** ---------------------------------------------------------------------------
    Finds mean of a 2D array across first index (in_loops)
    M is in/out_loops and N is time_steps
----------------------------------------------------------------------------*/
float* find_2d_mean(float** matrix, int num_loops, int time_steps)
{
    int j;
    float* avg = new float[time_steps];
    float sum = 0.0f;

    for (int i = 0; i < time_steps; i++)
    {
        for (j = 0; j < num_loops; j++)
        {
            sum += matrix[j][i];
        }

        // Calculating average across columns
        avg[i] = sum / num_loops;
        sum = 0.0f;
    }

    return avg;
}

/** ---------------------------------------------------------------------------
    Generates a random number seeded by system clock based on standard
    normal distribution on taking mean 0.0 and standard deviation 1.0
----------------------------------------------------------------------------*/
float rand_gen(float mean, float std_dev)
{
    // srand time(NULL);
    // int random = rand()%1 - 1;

    auto seed = chrono::system_clock::now().time_since_epoch().count();
    default_random_engine generator(static_cast<unsigned int>(seed));
    normal_distribution<float> distribution(mean, std_dev);
    return distribution(generator);
}

//----------------------------------------------------------------------------
// Simulates Black Scholes model
//----------------------------------------------------------------------------
float* run_black_scholes_model(float spot_price, int time_steps, float risk_rate, float volatility)
{
    float  mean = 0.0f, std_dev = 1.0f;                                             // Mean and standard deviation
    float  deltaT = 1.0f / time_steps;                                              // Timestep
    unique_ptr<float[]> norm_rand = make_unique<float[]>(time_steps - 1);           // Array of normally distributed random nos.
    float* stock_price = new float[time_steps];                                     // Array of stock price at diff. times
    stock_price[0] = spot_price;                                                    // Stock price at t=0 is spot price

    // Populate array with random nos.
    for (int i = 0; i < time_steps - 1; i++)
        norm_rand[i] = rand_gen(mean, std_dev);

    // Apply Black Scholes equation to calculate stock price at next timestep
    for (int i = 0; i < time_steps - 1; i++)
        stock_price[i + 1] = stock_price[i] * exp(((risk_rate - (powf(volatility, 2.0f) / 2.0f)) * deltaT) + (volatility * norm_rand[i] * sqrtf(deltaT)));

    return stock_price;
}

//----------------------------------------------------------------------------
// Main function
//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    clock_t t = clock();

    int in_loops = 100;     // Inner loop iterations
    int out_loops = 10000;  // Outer loop iterations
    int time_steps = 180;   // Stock market time-intervals (min)

    // Matrix for stock-price vectors per iteration
    float** stock = new float* [in_loops];
    for (int i = 0; i < in_loops; i++)
        stock[i] = new float[time_steps];

    // Matrix for mean of stock-price vectors per iteration
    float** avg_stock = new float* [out_loops];
    for (int i = 0; i < out_loops; i++)
        avg_stock[i] = new float[time_steps];

    // Vector for most likely outcome stock price
    float* opt_stock = new float[time_steps];

    float risk_rate = 0.001f;   // Risk free interest rate (%)
    float spot_price = 100.0f;  // Spot price (at t = 0)

    // Market volatility (calculated from data.csv)
    float volatility = calculate_volatility(spot_price, time_steps);

    // Welcome message
    cout << "--Welcome to Stockast: Stock Forecasting Tool--\n";
    cout << "  Copyright (c) 2017-2020 Rajdeep Konwar\n\n";
    cout << "  Using market volatility = " << volatility << endl;

    int i;
  
        for (i = 0; i < out_loops; i++)
        {
            /** Using Black Scholes model to get stock price every iteration
                Returns data as a column vector having rows=time_steps  **/
            for (int j = 0; j < in_loops; j++)
                stock[j] = run_black_scholes_model(spot_price, time_steps, risk_rate, volatility); //1000000

            // Stores average of all estimated stock-price arrays
            avg_stock[i] = find_2d_mean(stock, in_loops, time_steps);
        }

    // Average of all the average arrays
    opt_stock = find_2d_mean(avg_stock, out_loops, time_steps);

    // Write optimal outcome to disk
    ofstream file_ptr;
    file_ptr.open("opt.csv", ofstream::out);
    if (!file_ptr.is_open())
    {
        cerr << "Couldn't open opt.csv! Exiting..\n";
        return EXIT_FAILURE;
    }

    for (i = 0; i < time_steps; i++)
        file_ptr << opt_stock[i] << "\n";
    file_ptr.close();

    for (i = 0; i < in_loops; i++)
        delete[] stock[i];
    delete[] stock;

    for (i = 0; i < out_loops; i++)
        delete[] avg_stock[i];
    delete[] avg_stock;

    delete[] opt_stock;

    t = clock() - t;
    cout << " done!\n  Time taken = " << static_cast<float>(t / CLOCKS_PER_SEC) << "s";

    return getchar();
}