// RUN: %empty-directory(%t)
// RUN: %target-typecheck-verify-swift -swift-version 5 -enable-library-evolution -enable-implicit-dynamic -I %t

// REQUIRES: objc_interop

struct Container {
  var property: Int { return 1 }
  func foo() {}
}

class AClass {
  var property: Int { return 1 }
  func foo() {}

  @objc dynamic func allowed() {}


  @objc dynamic var allowedProperty : Int { return 1}
}
