using System;
using System.Reflection;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;

using ResponseRulesCLI;
using Tier1CLI.Containers;

namespace ResponseEditor
{
    /// <summary>
    /// Wraps the ResponseSystemCLI with some additional glue that makes the WPF go. 
    /// </summary> 
    public class ResponseSystemWPF : ResponseRulesCLI.ResponseSystemCLI
    {
        #region Interface Wrappers
        public Tier1CLI.Containers.INotifiableList ResponseGroupBinding
        {
            get { return ResponseGroupsDict; }
        }

        public Tier1CLI.Containers.INotifiableList CriteriaBinding
        {
            get { return CriteriaDict; }
        }

        /*
        public Tier1CLI.Containers.INotifiableList EnumerationsBinding
        {
            get { return EnumerationsDict; }
        }
        */

        public RulesAsList RulesBinding
        {
            get { return Rules; }
        }

        public override void LoadFromFile( String filename )
        {
            base.LoadFromFile(filename);

            ResponseGroupBinding.OnCollectionChanged(new NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction.Reset));
            CriteriaBinding.OnCollectionChanged(new NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction.Reset));
            RulesBinding.OnCollectionChanged(new NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction.Reset));
            
            // EnumerationsBinding.OnCollectionChanged(new NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction.Reset));

            RefreshShadowLists();
        }
        #endregion

        public ResponseSystemWPF()
        {
            ResponseRulesCLI.SingletonResponseSystem_t.RS = this;
            CriteriaBinding.CollectionChanged += this.OnCriteriaChanged;
            m_concepts = new ObservableCollection<string>();
        }

        #region Shadow Lists
        public ObservableCollection<String> ConceptsShadow
        {
            get { return m_concepts;  }
        }

        void OnCriteriaChanged( object sender, NotifyCollectionChangedEventArgs args )
        {
            RefreshShadowLists();
        }

        void RefreshShadowLists() 
        {
            // Do the concepts
            m_concepts.Clear();
            List<string> tempconcepts = new List<string>();
            // use reflection to fish out the Val prop.
            PropertyInfo valprop = CriteriaBinding[0].GetType().GetProperty("Val");

            foreach ( Object tupletype in CriteriaBinding )
            {
                Criterion crit = (Criterion)valprop.GetValue(tupletype, null);
                if ( crit.Key.Equals("concept", StringComparison.OrdinalIgnoreCase ) )
                {
                    tempconcepts.Add(crit.Value);
                }
            }
            tempconcepts.Sort();
            foreach ( string s in tempconcepts )
            {
                m_concepts.Add(s);
            }
            
        }

        #endregion

        #region Local private data.
        // Here's some local conveniences for making the debugger pane.
        ObservableCollection<String> m_concepts;
        #endregion

    }
}