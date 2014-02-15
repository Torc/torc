.pragma library

// A simple stack function that mimics/replicates QStack
function Stack() {
    var stack = [];

    this.clear = function() {
        stack = [];
    }

    this.push = function(element) {
        stack.push(element);
    };

    this.pop = function() {
        stack.pop();
    };

    this.size = function() {
        return stack.length;
    }

    this.top = function() {
        if (stack.length > 0) {
            return stack[stack.length - 1];
        }
        return undefined;
    }
}
