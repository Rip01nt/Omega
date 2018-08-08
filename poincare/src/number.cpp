#include <poincare/number.h>
#include <poincare/decimal.h>
#include <poincare/integer.h>
#include <poincare/rational.h>
#include <poincare/float.h>
#include <poincare/undefined.h>
#include <poincare/infinity.h>

extern "C" {
#include <stdlib.h>
#include <assert.h>
}
#include <cmath>

namespace Poincare {

double NumberNode::doubleApproximation() const {
  switch (type()) {
    case Type::Undefined:
      return NAN;
    case Type::Infinity:
      return sign() == Sign::Negative ? -INFINITY : INFINITY;
    case Type::Float:
      if (sizeof(*this) == sizeof(FloatNode<float>)) {
        return static_cast<const FloatNode<float> *>(this)->value();
      } else {
        assert(sizeof(*this) == sizeof(FloatNode<double>));
        return static_cast<const FloatNode<double> *>(this)->value();
      }
    case Type::Rational:
      return static_cast<const RationalNode *>(this)->templatedApproximate<double>();
    case Type::Integer:
      return static_cast<const IntegerNode *>(this)->templatedApproximate<double>();
    default:
      assert(false);
      return 0.0;
  }
}

static inline Number CreateNumber(double d) {
  if (std::isnan(d)) {
    return Undefined();
  } else if (std::isinf(d)) {
    return Infinity(d < 0.0);
  } else {
    return Float<double>(d);
  }
}

Number Number::Integer(const char * digits, size_t length, bool negative) {
  Integer i(digits, length, negative);
  if (!i.isInfinity()) {
    return i;
  }
  if (digits != nullptr && digits[0] == '-') {
    negative = true;
    digits++;
  }
  if (length > Decimal::k_maxExponentLength) {
    return Infinity(negative);
  }
  int exponent = Decimal::Exponent(digits, length, nullptr, 0, nullptr, 0, negative);
  return Decimal(digits, length, nullptr, 0, negative, exponent);
}

template <typename T> static Number Number::Decimal(T f) {
  if (std::isnan(f)) {
    return Undefined();
  }
  if (std::isinf(f)) {
    return Infinite(f < 0.0);
  }
  return Decimal(f);
}

Number Number::BinaryOperation(const Number i, const Number j, IntegerBinaryOperation integerOp, RationalBinaryOperation rationalOp, DoubleBinaryOperation doubleOp) {
  if (i.node()->type() == ExpressionNode::Type::Integer && j.node()->type() == ExpressionNode::Type::Integer) {
  // Integer + Integer
    Integer k = integerOp(Integer(i.node()), Integer(j.node()));
    if (!k.isInfinity()) {
      return k;
    }
  } else if (i.node()->type() == ExpressionNode::Type::Integer && j.node()->type() == ExpressionNode::Type::Rational) {
  // Integer + Rational
    Rational r = rationalOp(Rational(Integer(i.node())), Rational(j.node()));
    if (!r.numeratorOrDenominatorIsInfinity()) {
      return r;
    }
  } else if (i.node()->type() == ExpressionNode::Type::Rational && j.node()->type() == ExpressionNode::Type::Integer) {
  // Rational + Integer
    return Number::BinaryOperation(j, i, integerOp, rationalOp, doubleOp);
  } else if (i.node()->type() == ExpressionNode::Type::Rational && j.node()->type() == ExpressionNode::Type::Rational) {
  // Rational + Rational
    Rational a = rationalOp(Rational(i.node()), Rational(j.node()));
    if (!a.numeratorOrDenominatorIsInfinity()) {
      return a;
    }
  }
  // one of the operand is Undefined/Infinity/Float or the Integer/Rational addition overflowed
  double a = doubleOp(i.numberNode()->doubleApproximation(), j.numberNode()->doubleApproximation());
  return CreateNumber(a);
}

Number Number::Addition(const Number i, const Number j) {
  return BinaryOperation(i, j, Integer::Addition, Rational::Addition, [](double a, double b) { return a+b; });
}

Number Number::Multiplication(const Number i, const Number j) {
  return BinaryOperation(i, j, Integer::Multiplication, Rational::Multiplication, [](double a, double b) { return a*b; });
}

Number Number::Power(const Number i, const Number j) {
  return BinaryOperation(i, j, Integer::Power,
      // Special case for Rational^Rational: we escape to Float if the index is not an Integer
      [](const Rational i, const Rational j) {
        // We return an overflown result to reach the escape case Float+Float
        return Rational(Integer::Overflow());
      },
      [](double a, double b) {
        return std::pow(a, b);
      }
    );
}

}