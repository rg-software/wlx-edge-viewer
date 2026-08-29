=========================================
ReStructuredText (rst): plain text markup
=========================================

.. sectnum::

.. contents:: The tiny table of contents

What is reStructuredText?
~~~~~~~~~~~~~~~~~~~~~~~~~

An easy-to-read, what-you-see-is-what-you-get plaintext markup syntax
and parser system, abbreviated *rst*. In other words, using a simple
text editor, documents can be created which

- are easy to read in text editor and
- can be *automatically* converted to
 
  - html and 
  - latex (and therefore pdf)

What is it good for?
~~~~~~~~~~~~~~~~~~~~

reStructuredText can be used, for example, to

- write technical documentation (so that it can easily be offered as a
  pdf file or a web page)

- create html webpages without knowing html 

- to document source code

Show me some formatting examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can highlight text in *italics* or, to provide even more emphasis
in **bold**. Often, when describing computer code, we like to use a
``fixed space font`` to quote code snippets.

We can also include footnotes [1]_. We could include source code files
(by specifying their name) which is useful when documenting code. We
can also copy source code verbatim (i.e. include it in the rst
document) like this::

  int main ( int argc, char *argv[] ) {
      printf("Hello World\n");
      return 0;
  }

We have already seen at itemised list in section `What is it good
for?`_. Enumerated list and descriptive lists are supported as
well. It provides very good support for including html-links in a
variety of ways. Any section and subsections defined can be linked to,
as well.

Where can I learn more?
~~~~~~~~~~~~~~~~~~~~~~~

reStructuredText is described at
http://docutils.sourceforge.net/rst.html. We provide some geeky small
print in this footnote [2]_.

Show me some more stuff, please
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We can also include figures and images:

.. figure:: images/red.png
   :width: 300pt

   The magnetisation in a small ferromagnetic disk. The diametre is of
   the order of 120 nanometers and the material is Ni20Fe80. Png is a
   file format that is both acceptable for html pages as well as for
   (pdf)latex.

A plain (non-figure) image resolves against the document folder too:

.. image:: images/blue.png
   :alt: a blue square
   :width: 120px

Syntax-highlighted code
~~~~~~~~~~~~~~~~~~~~~~~

By declaring a language with the ``.. code::`` directive, the block is
coloured by highlight.js. Note that rst-compiler emits the block with
no language class, so the loader maps the declared language onto the
``<pre>`` so highlight.js can match a grammar:

.. code:: python

   def fib(n):
       return n if n < 2 else fib(n - 1) + fib(n - 2)

   print([fib(i) for i in range(10)])

The ``code-block`` alias works the same way:

.. code-block:: javascript

   const greeting = (name) => `Hello, ${name}!`;
   console.log(greeting("World"));

Math
~~~~

Block math via the ``.. math::`` directive is typeset by KaTeX:

.. math::

   \int_0^1 x^2 \, dx = \frac{1}{3}

Inline math uses the ``:math:`` role:

Euler's identity is :math:`e^{i\pi} + 1 = 0`.

Tables
~~~~~~

Both grid tables and simple tables render as real HTML tables.

Grid table:

+------------+------------+-----------+
| Header 1   | Header 2   | Header 3  |
+============+============+===========+
| row 1, c1  | row 1, c2  | row 1, c3 |
+------------+------------+-----------+
| row 2, c1  | row 2, c2  | row 2, c3 |
+------------+------------+-----------+

Simple table:

=====  =====  =====
A      B      C
=====  =====  =====
1      2      3
4      5      6
=====  =====  =====

Admonitions
~~~~~~~~~~~

Admonitions are styled boxes:

.. note::

   This is a note. It draws attention to something.

.. warning::

   This is a warning. Be careful.

.. tip::

   This is a tip. Handy advice.

Definition lists
~~~~~~~~~~~~~~~~

Definition lists are supported:

term one
   The definition of *term one*.

term two
   The definition of *term two*.

Block quotes
~~~~~~~~~~~~

A block quote indents and styles quoted text:

   This whole paragraph is indented and rendered as a block quote,
   useful for quoted material.

Cross references
~~~~~~~~~~~~~~~~

In-page cross references tie into anchors, so clicking the link below
scrolls to the target section:

- See the :ref:`syntax-highlighted-code` section.
- See the :ref:`show-me-some-more-stuff-please` section.

.. _syntax-highlighted-code:

Syntax-highlighted code
~~~~~~~~~~~~~~~~~~~~~~~

(The section heading above is the anchor target for the ``:ref:`` role.)

You can navigate to another document in the same folder by clicking a
relative ``.rst`` link; it is rendered in the same view, see the
`Navigation`_ section below.

Navigation
~~~~~~~~~~

.. _Navigation:

Open the linked document to verify same-folder navigation:
`RstNavTarget.rst`_.

---------------------------------------------------------------------------

.. [1] although there isn't much point of using a footnote here.

.. [2] Random facts: 

  - Emacs provides an rst mode 
  - when converting rst to html, a style sheet can be provided (there is a similar feature for latex)
  - rst can also be converted into XML
  - the recommended file extension for rst is ``.txt``
