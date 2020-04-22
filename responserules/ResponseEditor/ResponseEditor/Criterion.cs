using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace ResponseEditor
{
    /// <summary>
    /// Represents a single AI_Criterion inside a rule. 
    /// </summary>
    /// <remarks>
    /// A response rule contains a list of criteria, each of which consists of a key,
    /// a matcher (comparator), and a value. A query into the response system contains a 
    /// list of key:value pairs. Each of these is tested against all the criteria in a rule.
    /// Each matching criterion increases that rule's score. The best scoring rule is selected.
    /// If a criterion is marked as Required, then its failure rejects the rule.
    /// Otherwise it just doesn't contribute to the score.
    /// </remarks>
    public class DummyCriterion
    {
        /// <summary>
        /// Encapsulates the comparison of a value against this criterion.
        /// </summary>
        /// <remarks>
        /// The Matcher performs the comparison between the value specified in 
        /// the criterion and the value specified in a context. Eg, a criterion
        /// with key "foo", value 5 and matcher ">" means that only queries with
        /// context "foo" greater than 5 will match this criterion.
        /// 
        /// Right now this is a complete mockup since the actual matching code is all in the 
        /// C++ side.
        /// </remarks>
        public class Matcher
        {
            public Matcher(string text)
            {
                this.text = text;
            }

            /// <summary>
            /// Test to see if a given value from a query matches this criterion
            /// </summary>
            /// <param name="crit">The criterion to be matched (usually contains this matcher)</param>
            /// <param name="queryValue">The value to be tested, AKA the context.</param>
            bool Test( DummyCriterion crit, string queryValue)
            {
                throw new System.NotImplementedException("DummyCriterion::Matcher::Test() is not actually implemented.");
            }


            /// <summary>
            /// The actual text of the matcher from the source file. May be null (implies ==)
            /// </summary>
            private string text;

            public string Text
            {
                get { return text; }
                set { text = value; }
            }

            public override string ToString()
            {
                return Text;
            }
        };

        public DummyCriterion(string key, string value, float weight, bool required, Matcher comparison)
        {
            this.key = key;
            this.value = value;
            this.weight = weight;
            this.required = required;
            this.comparison = comparison;
        }

        private string key;
        private string value;
        private float weight;
        private bool required;
        private Matcher comparison;

        public string Key
        {
            get { return key; }
            // set { key = value; }
        }

        public string Value
        {
            get { return this.value; }
            // set { this.value = value; }
        }

        public bool Required
        {
            get { return required; }
            // set { required = value; }
        }

        public Matcher Comparison
        {
            get { return comparison; }
            // set { comparison = value; }
        }

        public float Weight
        {
            get { return weight; }
            // set { weight = value; }
        }


        /// dummy criteria data for testing
        public static DummyCriterion[] g_DummyCriteria = 
        { 
            new DummyCriterion( "foo",       "1",    1, false, new Matcher(">") ),
            new DummyCriterion( "bar",       "soup", 1, false, new Matcher("")  ),
            new DummyCriterion( "Concept",   "Talk", 1, true,  new Matcher("")  )
        };
    }


}
