# How to contribute

Third-party contributions are essential for the future development of MikuMikuPenguin.
We want to keep it as easy as possible to contribute changes that get things working
in your environment. There are a few guidelines that we need contributors to follow
so that we can keep on top of things.

## Getting Started

* Make sure you have a [GitHub account](https://github.com/signup/free).
* Create an Issue for your problem, assuming one does not already exist.
  * Clearly describe the issue including steps to reproduce when it is a bug.
* Fork the repository on GitHub.

## Making Changes

* Create a topic branch from where you want to base your work.
  * This is usually the master branch.
  * To quickly create a topic branch based on master; `git branch
    my_contribution master` then checkout the new branch with `git
    checkout my_contribution`. Please avoid working directly on the
    `master` branch.
* Make commits of logical units.
* Check for unnecessary whitespace with `git diff --check` before committing.
* Make sure your commit messages are in the proper format.

````
Essential commit message information here.

The first line summarizes the patch content.
The body paragraph describes the behavior without the patch,
why this is a problem, and how the patch fixes the problem when applied.
````

* Make sure you have added the necessary tests for your changes.
* Run _all_ the tests to assure nothing else was accidentally broken.

## Coding Style
In general, please use coding and naming syntax similar to the following example when possible.
It is not required to code in this style, but it is preferred.
```cpp
int variableName;

class ClassName
{
	public:
	ClassName();
	~ClassName();

	void classFunctionName();

	private:
	int classVariableName;
};

void functionName()
{
	//function contents...
}
```

In general, 'variableName' is used rather than 'variable_name' for variables and functions,
and the first letter (The 'C' in 'ClassName') of class names is capitalized.
Brackets are typically opened and closed on a separate line, rather than:
```cpp
void functionName() { //Bracket is opened on the same line as the class/function name. Typically unused style in MMP code
	//Function contents here...
};
```

## Making Trivial Changes

### Documentation

For changes of a trivial nature to comments and documentation, it is
appropriate to start the first line of a commit with '(doc)'.

````
(doc) Add documentation commit example to CONTRIBUTING

There is no example for contributing a documentation commit
to the MMP repository. This is a problem because the contributor
is left to assume how a commit of this nature may appear.

The first line summarizes the patch content, starting with '(doc)'
at the beginning of the commit. The body describes the nature of
the new documentation or comments added.
````

## Submitting Changes

* Push your changes to a topic branch in your fork of the repository.
* Submit a pull request to the MikuMikuPenguin repository.

# Additional Resources

* [General GitHub documentation](http://help.github.com/)
* [GitHub pull request documentation](http://help.github.com/send-pull-requests/)
* sn0w75 on Twitter (http://twitter.com/sn0w75), e-mail (ibenrunnin@gmail.com)

