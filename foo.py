import gensim.downloader as api
import numpy as np

# Load a pre-trained word embedding model (Word2Vec)
model = api.load("word2vec-google-news-300")  # 300D Word2Vec trained on Google News

# Get the vector for 'russian' (as a language)
word = "russian"
if word in model:
    v = model[word]  # Get vector
    v_neg = -v  # Compute the negation of the vector

    # Find the most distant word (highest cosine similarity with -v)
    most_distant = model.similar_by_vector(v_neg, topn=10)
    
    print("Most conceptually distant words from 'russian':")
    for word, similarity in most_distant:
        print(f"{word}: {similarity:.4f}")
else:
    print(f"'{word}' not found in the model vocabulary.")
