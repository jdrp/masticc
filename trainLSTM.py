import numpy as np
import pandas as pd
import random
import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler, MinMaxScaler
import torch
import torch.nn as nn
from torch.autograd import Variable


mm = MinMaxScaler()
ss = StandardScaler()
dataset_name = 'training_data/latency_data.csv'

def splitData(df, bucket_size):
    nr_entries = len(df)
    nr_buckets = (int)(nr_entries/bucket_size)
    nr_buckets_test = int(nr_buckets/10)
    if(nr_buckets_test <= 0):
        nr_buckets_test = 1
    nr_buckets_validation = int(nr_buckets/10)
    if(nr_buckets_validation <= 0):
        nr_buckets_validation = 1

    bucket_division = dict()
    while(len(bucket_division) < nr_buckets_test):
        randomizer = random.randrange(nr_buckets)
        if (randomizer not in bucket_division.keys()):
            bucket_division[randomizer] = 'test'

    while(len(bucket_division) < (nr_buckets_test + nr_buckets_validation)):
        randomizer = random.randrange(nr_buckets)
        if (randomizer not in bucket_division.keys()):
            bucket_division[randomizer] = 'validation'

    df_train = pd.DataFrame()
    df_test = pd.DataFrame();
    df_validation = pd.DataFrame();
    for i in range(nr_buckets):
        if (i in bucket_division.keys()):
            if (bucket_division[i] == 'test'):
                df_test = pd.concat([df_test, df.iloc[i*bucket_size:(i+1)*bucket_size]])
            if (bucket_division[i] == 'validation'):
                df_validation = pd.concat([df_validation, df.iloc[i*bucket_size:(i+1)*bucket_size]])
        else:
            df_train = pd.concat([df_train, df.iloc[i*bucket_size:(i+1)*bucket_size]])

    print("Split data into " + str(nr_buckets) + " buckets of size " + str(bucket_size))
    print("  training:\t" + str(nr_buckets - nr_buckets_test - nr_buckets_validation) + " buckets")
    print("  testing:\t" + str(nr_buckets_test) + " buckets")
    print("  validation:\t" + str(nr_buckets_validation) + " buckets\n")


    if (True):
        df_train_plot = df_train.reset_index(drop=True)
        df_test_plot = df_test.reset_index(drop=True)
        df_validation_plot = df_validation.reset_index(drop=True)
        plot([df_train_plot.iloc[:, -1:]], ['training'], 'training data')
        plot([df_test_plot.iloc[:, -1:]], ['testing'], 'testing data')
        plot([df_validation_plot.iloc[:, -1:]], ['valadtion'], 'valadtion data')

    return df_train, df_test, df_validation

def processData(df):
    mm = MinMaxScaler()
    ss = StandardScaler()

    X = df.iloc[:, :-1]
    y = df.iloc[:, -1:]

    ss = ss.fit_transform(X)
    mm = mm.fit_transform(y)

    X_scaled = ss[:len(df), :]
    X_tensors = Variable(torch.Tensor(X_scaled))
    X_tensors = torch.reshape(X_tensors,  (X_tensors.shape[0], 1, X_tensors.shape[1]))
    y_scaled = mm[:len(df), :]
    y_tensors = Variable(torch.Tensor(y_scaled))

    return X_tensors , y_tensors

def getTrainingData():
    df = pd.read_csv(dataset_name, index_col = 'ts')
    #plt.style.use('ggplot');
    #df['gt'].plot(label='ts', title='bandwithratio over time')
    #plt.show()

    X = df.iloc[:, :-1]
    y = df.iloc[:, -1:]

    X_ss = ss.fit_transform(X)
    y_mm = mm.fit_transform(y)

    X_train = X_ss[:10, :]
    X_test = X_ss[10:, :]

    y_train = y_mm[:10, :]
    y_test = y_mm[10:, :]

    X_train_tensors = Variable(torch.Tensor(X_train))
    X_test_tensors = Variable(torch.Tensor(X_test))

    y_train_tensors = Variable(torch.Tensor(y_train))
    y_test_tensors = Variable(torch.Tensor(y_test))

    X_train_tensors_final = torch.reshape(X_train_tensors,   (X_train_tensors.shape[0], 1, X_train_tensors.shape[1]))
    X_test_tensors_final = torch.reshape(X_test_tensors,  (X_test_tensors.shape[0], 1, X_test_tensors.shape[1]))

    #print("Training Shape", X_train_tensors_final.shape, y_train_tensors.shape)
    #print("Testing Shape", X_test_tensors_final.shape, y_test_tensors.shape)

    return X_train_tensors_final, y_train_tensors, X_test_tensors_final, y_test_tensors

def trainModel(df_train):
    X_train, y_train = processData(df_train)

    num_epochs = 100000 #1000 epochs
    learning_rate = 0.001 #0.001 lr

    input_size = 7 #number of features
    hidden_size = 10 #number of features in hidden state
    num_layers = 1 #number of stacked lstm layers

    num_classes = 1 #number of output classes

    lstm1 = LSTM1(num_classes, input_size, hidden_size, num_layers, X_train.shape[1]) #our lstm class

    criterion = torch.nn.MSELoss()    # mean-squared error for regression
    optimizer = torch.optim.Adam(lstm1.parameters(), lr=learning_rate)

    for epoch in range(num_epochs):
        outputs = lstm1.forward(X_train) #forward pass
        optimizer.zero_grad() #caluclate the gradient, manually setting to 0

        # obtain the loss function
        loss = criterion(outputs, y_train)

        loss.backward() #calculates the loss of the loss function

        optimizer.step() #improve from loss, i.e backprop
        if epoch % 100 == 0:
            print("Epoch: %d, loss: %1.5f" % (epoch, loss.item()))

    return lstm1

def testModel(lstm, df_test):
    X_test, y_test = processData(df_test)
    train_predict = lstm(X_test)#forward pass
    data_predict = train_predict.data.numpy() #numpy conversion
    dataY_plot = y_test.data.numpy()

    mm.fit_transform(data_predict)
    data_predict = mm.inverse_transform(data_predict) #reverse transformation
    # mm.fit_transform(dataY_plot)
    dataY_plot = mm.inverse_transform(dataY_plot)

    return data_predict, dataY_plot


class LSTM1(nn.Module):
    def __init__(self, num_classes, input_size, hidden_size, num_layers, seq_length):
        super(LSTM1, self).__init__()
        self.num_classes = num_classes #number of classes
        self.num_layers = num_layers #number of layers
        self.input_size = input_size #input size
        self.hidden_size = hidden_size #hidden state
        self.seq_length = seq_length #sequence length

        self.lstm = nn.LSTM(input_size=input_size, hidden_size=hidden_size,
                          num_layers=num_layers, batch_first=True) #lstm
        self.fc_1 =  nn.Linear(hidden_size, hidden_size) #fully connected 1
        self.fc_2 =  nn.Linear(hidden_size, hidden_size) #fully connected 2
        self.fc_3 =  nn.Linear(hidden_size, hidden_size) #fully connected 3
        self.fc = nn.Linear(hidden_size, num_classes) #fully connected last layer

        self.relu = nn.ReLU()

    def forward(self,x):
        h_0 = Variable(torch.zeros(self.num_layers, x.size(0), self.hidden_size)) #hidden state
        c_0 = Variable(torch.zeros(self.num_layers, x.size(0), self.hidden_size)) #internal state
        # Propagate input through LSTM
        output, (hn, cn) = self.lstm(x, (h_0, c_0)) #lstm with input, hidden, and internal state
        hn = hn.view(-1, self.hidden_size) #reshaping the data for Dense layer next
        out = self.relu(hn)
        out = self.fc_1(out) #first Dense
        out = self.fc_2(out) #second Dense
        out = self.fc_3(out) #third Dense
        out = self.relu(out) #relu
        out = self.fc(out) #Final Output
        return out

def plot(datasets, labels, title):
    plt.figure(figsize=(10,6)) #plotting
    # plt.axvline(x=20, c='r', linestyle='--') #size of the training set
    if (len(datasets) != len(labels)):
        print("Labels did not match ")
        return
    for i in range(len(datasets)):
        plt.plot(datasets[i], label=labels[i])
    plt.title(title)
    plt.legend()


def main():
    df = pd.read_csv(dataset_name, index_col = 'ts')
    df_train, df_test, df_validation = splitData(df, 100)
    plt.show()

    lstm = trainModel(df_train)
    data_predict, dataY_plot = testModel(lstm, df_test)
    plot([dataY_plot, data_predict],['Actuall Data', 'Predicted Data'],'Time-Series Prediction')

    # X_test, y_test = processData(df_test)
    # example_data = torch.Tensor(1,1,1,1,1,1,1)
    # traced_script_module = torch.jit.trace(lstm, X_test)
    # traced_script_module.save("abc.pt")
    plt.show()

    torch.save(lstm.state_dict(), "./savedModel1.pth")
main()
