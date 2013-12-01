void Sequence_Stats(string sequence)
{
    uint cg_counter = 0;
    uint c_counter = 0;
    uint g_counter = 0;
    uint cpg_counter = 0;
    uint i = 0;
    for (; i < sequence.length() - 1; ++i)
    {
        if (sequence[i] == 'c')
        {
            c_counter += 1;
            cg_counter += 1;
        }

        if (sequence[i] == 'g')
        {
            g_counter += 1;
            cg_counter += 1;
        }

        if (sequence[i] == 'c' && sequence[i + 1] == 'g')
        {
            cpg_counter += 1;
        }
    }

    if (sequence[i] == 'c')
    {
        c_counter += 1;
        cg_counter += 1;
    }

    if (sequence[i] == 'g')
    {
        g_counter += 1;
        cg_counter += 1;
    }

    cerr << "Sequence length: " << sequence.length() << endl;
    cerr << "CG percenage: " << setprecision(3) << cg_counter/(double)sequence.length()*100 << "%" << endl;
    cerr << "Observed to expected CpG ratio: " << setprecision(3)
         << cpg_counter/(double)(c_counter*g_counter)*sequence.length()*100 << "%" << endl;
}

int main(int argc, char **args)
{
    ifstream input_sequence("input_sequence.txt");
    ifstream input_initial_probabilities("initial_probabilities.txt");
    ifstream input_transition_probabilities("transition_probabilities.txt");
    ifstream input_emission_probabilities("emission_probabilities.txt");
    string next_value;
    map<char, ushort> nucleotides_mapping;
    vector<char> state_mapping(8);
    nucleotides_mapping['a'] = 0;
    nucleotides_mapping['c'] = 1;
    nucleotides_mapping['g'] = 2;
    nucleotides_mapping['t'] = 3;
    state_mapping[0] = 'A';
    state_mapping[1] = 'C';
    state_mapping[2] = 'G';
    state_mapping[3] = 'T';
    state_mapping[4] = 'a';
    state_mapping[5] = 'c';
    state_mapping[6] = 'g';
    state_mapping[7] = 't';
    string observed;

    string line;
    while (!input_sequence.eof())
    {
        getline(input_sequence, line);
        observed += line;
    }

    input_sequence.close();

    transform(observed.begin(), observed.end(), observed.begin(), ::tolower);

    Sequence_Stats(observed);

    int number_of_states = 8;
    int alphabet_size = 4;
    ull average_cpg_island_length = 20;
    ull average_non_cpg_island_length = 50;

    double stay_in_cpg_probability = 1 - 1/(double)average_cpg_island_length;
    double stay_in_non_cpg_probability = 1 - 1/(double)average_non_cpg_island_length;

    boost::shared_ptr<HMMVector<double> > initial_probabilities_sptr(new HMMVector<double>(number_of_states));
    boost::shared_ptr<HMMMatrix<double> > transition_probabilities_sptr(new HMMMatrix<double>(number_of_states,
                                                                                              number_of_states));
    boost::shared_ptr<HMMMatrix<double> > emission_probabilities_sptr(new HMMMatrix<double>(alphabet_size,
                                                                                            number_of_states));

    HMMVector<double> &initial_probabilities = *initial_probabilities_sptr;
    // initial probabilities
    ushort i = 0;
    for (ushort i = 0; i < number_of_states; ++i)
    {
        input_initial_probabilities >> next_value;
        initial_probabilities(i) = atof(next_value.c_str());
    }

    input_initial_probabilities.close();

    //transition probabilities
    HMMMatrix<double> cpg_transitions(number_of_states/2, number_of_states/2);
    for (ushort i = 0; i < number_of_states/2; ++i)
    {
        for (ushort j = 0; j < number_of_states/2; ++j)
        {
            input_transition_probabilities >> next_value;
            cpg_transitions(i, j) = atof(next_value.c_str());
        }
    }

    HMMMatrix<double> non_cpg_transitions(number_of_states/2, number_of_states/2);
    for (ushort i = 0; i < number_of_states/2; ++i)
    {
        for (ushort j = 0; j < number_of_states/2; ++j)
        {
            input_transition_probabilities >> next_value;
            non_cpg_transitions(i, j) = atof(next_value.c_str());
        }
    }

    input_transition_probabilities.close();

    HMMMatrix<double> &transition_probabilities = *transition_probabilities_sptr;
    for (ushort i = 0; i < number_of_states; ++i)
    {
        for (ushort j = 0; j < number_of_states; ++j)
        {
            if (i < number_of_states/2 && j < number_of_states/2)
            {
                transition_probabilities(i, j) = cpg_transitions(i,j) * stay_in_cpg_probability;
            }
            else if (i < number_of_states/2 && j >= number_of_states/2)
            {
                transition_probabilities(i, j) = non_cpg_transitions(i,j - number_of_states/2) *
                        (1 - stay_in_cpg_probability);
            }
            else if (i >= number_of_states/2 && j < number_of_states/2)
            {
                transition_probabilities(i, j) = cpg_transitions(i - number_of_states/2, j) *
                        (1 - stay_in_non_cpg_probability);
            }
            else
            {
                transition_probabilities(i, j) = non_cpg_transitions(i - number_of_states/2, j - number_of_states/2)
                        * stay_in_non_cpg_probability;
            }
        }
    }

    HMMMatrix<double> &emission_probabilities = *emission_probabilities_sptr;
    for (ushort i = 0; i < number_of_states; ++i)
    {
        for (ushort j = 0; j < alphabet_size; ++j)
        {
            input_emission_probabilities >> next_value;
            emission_probabilities(j, i) = atof(next_value.c_str());
        }
    }

    input_emission_probabilities.close();

    cerr << "Constructing HMM" << endl;
    HMM<double> hmm(initial_probabilities_sptr, transition_probabilities_sptr, emission_probabilities_sptr);

    sequence observed_sequence(observed.length());

    for (ull i = 0; i < observed.length(); ++i)
    {
        observed_sequence[i] = nucleotides_mapping[observed[i]];
    }

    cerr << "Running viterbi" << endl;
    sequence hidden_sequence(observed.length());
    double log_likelihood = hmm.viterbi(observed_sequence, hidden_sequence);

    for (ull i = 0; i < observed.length(); ++i)
    {
        cerr << state_mapping[hidden_sequence[i]];
    }

    cerr << "\nLog likelihood of hidden sequence: " << log_likelihood << endl;

    cerr << "Running forward" << endl;
    HMMMatrix<double> forward_dynamic(observed.length(), number_of_states);
    HMMVector<double> scales(observed.length());
    hmm.forward(observed_sequence, scales, forward_dynamic);

    cerr << "Running likelihood" << endl;
    log_likelihood = hmm.likelihood(scales);
    cerr << "Log likelihood of observed sequence: " << log_likelihood << endl;

    cerr << "Running backward" << endl;
    HMMMatrix<double> backward_dynamic(observed.length(), number_of_states);
    hmm.backward(observed_sequence, scales, backward_dynamic);

    cerr << "Running posterior decoding" << endl;
    HMMMatrix<double> posterior_decoding(observed.length(), number_of_states);
    hmm.posterior_decoding(observed_sequence, forward_dynamic, backward_dynamic, scales, posterior_decoding);

    cerr << "Running Baum-Welch" << endl;
    boost::shared_ptr<HMMVector<double> > new_initial_probabilities_sptr(new HMMVector<double>(number_of_states));
    boost::shared_ptr<HMMMatrix<double> > new_transition_probabilities_sptr(new HMMMatrix<double>(number_of_states,
                                                                                                  number_of_states));
    boost::shared_ptr<HMMMatrix<double> > new_emission_probabilities_sptr(new HMMMatrix<double>(alphabet_size,
                                                                                                number_of_states));
    hmm.baum_welch(observed_sequence, forward_dynamic, backward_dynamic, scales, *new_initial_probabilities_sptr,
                   *new_transition_probabilities_sptr, *new_emission_probabilities_sptr);

    cerr << "Constructing new HMM" << endl;
    HMM<double> new_hmm(new_initial_probabilities_sptr, new_transition_probabilities_sptr, new_emission_probabilities_sptr);

    cerr << "Running forward on new HMM" << endl;
    new_hmm.forward(observed_sequence, scales, forward_dynamic);
    cerr << "Running likelihood on new HMM" << endl;
    log_likelihood = new_hmm.likelihood(scales);
    cerr << "Log likelihood of observed sequence in new HMM: " << log_likelihood << endl;

    cerr << "Running viterbi" << endl;
    log_likelihood = new_hmm.viterbi(observed_sequence, hidden_sequence);

    for (ull i = 0; i < observed.length(); ++i)
    {
        cerr << state_mapping[hidden_sequence[i]];
    }

    cerr << "\nLog likelihood of hidden sequence: " << log_likelihood << endl;
}