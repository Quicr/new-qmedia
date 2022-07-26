# Build Pre-requisites for QMedia

## MacOS
These instructions are for MacOS and have been validated on Apple Silicon/M1.

### Dependencies
1. **xcode** - Install from App Store

   Make sure to validate xcode is installed and the license has been accepted. 
   ```
   sudo xcode-select --install
   sudo xcodebuild -license accept
   ```
   Launch **xcode** to make sure there are no other updates or components to install.
   

   > Not found ```c++``` library error will happen if **xcode** is not correctly install.  

2. **brew** - Follow the instructions at https://brew.sh/
3. **cmake** - Run ```brew install cmake```

### Setup vcpkg dependency

#### 1) Clone QMedia Repo

> Below uses SSH, you can clone via other methods
```
git clone git@github.com:Quicr/qmedia.git
```

#### 1) Init Submodules
Clones/updates the submodules. Currently ```vcpkg``` is the only module. Modules
are required to run ```cmake```

```
cd qmedia
git submodule init
git submodule update
```

### 2) Manually build/install vcpkg
Currently the ```vcpkg``` needs to be build/installed manually.  You can do that by following
the [vcpkg instructions](https://vcpkg.io/en/getting-started.html). 

For example: 

```
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
Downloading vcpkg-macos...
```
