# Pareco

Current Directory Structure
```
├── LICENSE
├── README.md
├── requirements.txt
├── package.json
├── package-lock.json
├── .jscpd.json
├── node_modules
├── NewPaReco
├── Methods
├── Pareco
└── 
```

# Setting up
To set up `Pareco` you will need to do the following steps:

# Minimum Requirements
``OS``: Windows, linux, MacOS
``RAM``: 4GB
``Storage``: 1GB
``Python``: 3.12
``PIP``: 24

# Download NPM
Follow the official documentation of NPM 
```
https://docs.npmjs.com/downloading-and-installing-node-js-and-npm
```

# Installing the packages
After having NPM installed run the following command to install JSCPD
```
npm install
```
This command will directly install of the provided package.json and package-lock.json files

# Install requirements
```
pip install -r requirements.txt
```

# Github Tokens
We use [GitHub tokens](https://github.com/settings/tokens) when extracting PR patches. This allows for higher rate limit because of the high number of requests to the GitHub API. Tokens can be set in the tokens.txt file seperated by a comman. The user can add as many tokens as needed. A minimal of 2 tokens can be used to safely execute code and to make sure that the rate limit is not reached for a token.