using System;
using System.IO;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using Microsoft.Win32;

using ResponseRulesCLI;
using ManagedAppSystem;

namespace ResponseEditor
{
    /// <summary>
    /// Interaction logic for Window1.xaml
    /// </summary>
    public partial class Window1 : Window
    {
        public Window1()
        {
            InitializeComponent();
            // m_RulesMatchingDebuggerCriterion = new ObservableCollection<Rule>();
        }

        private void button1_Click(object sender, RoutedEventArgs e)
        {
            // MessageBox.Show("CLICK!");
            // MessageBox.Show(Resources["AppSystem"].ToString());
            // AppSystemWrapper appSys = new AppSystemWrapper();
            // CritGrid.ItemsSource = RRSys.CriteriaBinding;
            // Resources["CritGrid"].ItemsSource = RRSys.CriteriaBinding;

            // get a filename from the dialog box.
            string filename = null;

            {
                // Configure open file dialog box
                Microsoft.Win32.OpenFileDialog dlg = new Microsoft.Win32.OpenFileDialog();
                dlg.FileName = "response_rules.txt"; // Default file name
                dlg.DefaultExt = ".txt"; // Default file extension
                dlg.Filter = "Text (.txt)|*.txt"; // Filter files by extension
                string searchdir = System.IO.Path.Combine(Environment.GetEnvironmentVariable("VGAME"),
                    Environment.GetEnvironmentVariable("VMOD"));
                if ( searchdir.Length > 4 )
                {
                    dlg.InitialDirectory = System.IO.Path.Combine(searchdir, "scripts/talker");
                }
                else
                {
                    dlg.InitialDirectory = Directory.GetCurrentDirectory();
                }

                // Show open file dialog box
                Nullable<bool> result = dlg.ShowDialog();

                // Process open file dialog box results
                if (result == true)
                {
                    // Open document
                    filename = dlg.FileName;
                }
                else
                {   // cancelled
                    return;
                }
            }

            if ( System.IO.File.Exists( filename ) )
            {
                // get the local modpath directory from the given filename.
                string basedir = System.IO.Path.GetDirectoryName(filename);

                // check to make sure that "scripts/talker" is in there somewhere, 
                // and if not, bitch.
                bool bIsScriptsTalkerDir = (basedir.Contains("scripts/talker") || basedir.Contains("scripts\\talker"));
                if ( !bIsScriptsTalkerDir )
                {
                    MessageBox.Show("File is not in a 'scripts/talker' directory; #includes may not work properly.",
                        "Minor Annoyance");
                }
                else
                {
                    // walk up two levels in the directory name. 
                    basedir = Directory.GetParent(basedir).Parent.FullName;
                }

                AppSys.SetFileSystemSearchRoot(basedir);

                // load just the bare file name.
                RRSys.LoadFromFile(filename);
            }
            else
            {
                MessageBox.Show("That was a bad file name; no responses loaded.");
            }


            /*
            foreach ( Object i in (System.Collections.IEnumerable)(RRSys.CriteriaBinding) )
            {
                Criterion c = (Criterion)i.GetType().GetProperty("Val").GetValue(i, null);
                // MessageBox.Show( c.GetType().GetProperty("Key").GetValue(c,null).ToString() );
            }
            */

        }

        #region Member data for all panes
        public ResponseSystemWPF RRSys
        {
            get { return (ResponseSystemWPF)this.FindResource("ResponseSystem"); }
        }
        public AppSystemWrapper AppSys
        {
            get { return (AppSystemWrapper)this.FindResource("AppSystem"); }
        }

        #endregion


        #region Criterion Pane
        private void CritGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {

        }
        #endregion


        #region Rules pane
        public void OnRulesTabLoaded(Object sender, RoutedEventArgs args)
        {
            ViewForCriteriaPanelInRulesPane = new ListCollectionView(RRSys.CriteriaBinding);
            ViewForResponsePanelInRulesPane = new ListCollectionView(RRSys.ResponseGroupBinding);
        }

        public ListCollectionView ViewForCriteriaPanelInRulesPane;
        public ListCollectionView ViewForResponsePanelInRulesPane;


        private void RulesGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count > 1)
            {
                MessageBox.Show("How can you select more than one rule?");
            }

            if (e.AddedItems.Count == 0)
            {
                ViewForCriteriaPanelInRulesPane.Filter = null;
                ViewForResponsePanelInRulesPane.Filter = null;
            }
            else
            {
                Rule r = e.AddedItems[0] as Rule;
                ViewForCriteriaPanelInRulesPane.Filter = MakeCriterionFilterForRule(r);
                ViewForResponsePanelInRulesPane.Filter = MakeResponseFilterForRule(r);
            }

            Binding b = new Binding();
            b.Source = ViewForCriteriaPanelInRulesPane;
            b.Mode = BindingMode.OneWay;
            CriteriaGridInRulesPane.SetBinding(ItemsControl.ItemsSourceProperty, b);

            b = new Binding();
            b.Source = ViewForResponsePanelInRulesPane;
            b.Mode = BindingMode.OneWay;
            ResponseGridInRulesPane.SetBinding(ItemsControl.ItemsSourceProperty, b);
        }

        private Predicate<object> MakeCriterionFilterForRule( Rule r )
        {
            // make a closure scanning the response dict for just this rule's criteria.
            ResponseSystemWPF ResponseRuleSystem = RRSys;
            return x =>
                {
                    int idx = ResponseRuleSystem.CriteriaBinding.IndexOf(x); ;
                    for ( int i = 0 ; i < r.NumCriteria ; i++ )
                    {
                        if (r.get_CriteriaIndices(i) == idx)
                            return true;
                    }
                    return false;
                };
        }

        private Predicate<object> MakeResponseFilterForRule(Rule r)
        {
            // make a closure scanning the response dict for just this rule's responses.
            ResponseSystemWPF ResponseRuleSystem = RRSys;
            
            // CLI seems to be having trouble with typedefs, so for now introspect the 
            // necesasry properties
            System.Reflection.PropertyInfo valprop = RRSys.ResponseGroupsDict[0].GetType().GetProperty("Val");

            return x =>
            {
                // clumsy hack to filter out NullResponse
                ResponseGroup g = valprop.GetValue(x, null) as ResponseGroup;
                if (g.Count < 1) 
                    return false;

                int idx = ResponseRuleSystem.ResponseGroupBinding.IndexOf(x);
                for (int i = 0; i < r.NumCriteria; i++)
                {
                    if (r.get_ResponseIndices(i) == idx)
                        return true;
                }
                return false;
            };
        }

        #endregion

        #region Debugger Pane
        public void OnDebuggerTabLoaded(Object sender, RoutedEventArgs args)
        {
            ViewForResponsePanelInDebuggerPane = new ListCollectionView(RRSys.ResponseGroupBinding);
        }


        public ListCollectionView ViewForResponsePanelInDebuggerPane;
        // private Binding BindingForResponseDetailInDebuggerPane;

        private void DebugTabConceptComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            // need to use event parameter rather than contents of combo box as
            // the combo box's text hasn't actually been set yet
            if ( e.AddedItems.Count > 0 )
            {
                DisplayBestRuleForConcept(e.AddedItems[0].ToString(), DebugTabFactsTextBox.Text);
            }
            else
            {     
                ObservableCollectionOfRules collection = (ObservableCollectionOfRules)this.FindResource("RulesMatchingDebuggerCriterion");
                collection.Clear();
            }
        }

        private void DebuggerRuleGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count == 0)
            {
                ViewForResponsePanelInDebuggerPane.Filter = null;
            }
            else
            {
                Rule r = ((KeyValuePair<Rule, float>) e.AddedItems[0]).Key as Rule;
                ViewForResponsePanelInDebuggerPane.Filter = MakeResponseFilterForRule(r);
            }


            Binding b = new Binding();
            b.Source = ViewForResponsePanelInDebuggerPane;
            b.Mode = BindingMode.OneWay;
            ResponseGridInDebuggerPane.SetBinding(ItemsControl.ItemsSourceProperty, b);
        }

        private void DebuggerResponseGroupGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            /*
            if (BindingForResponseDetailInDebuggerPane == null)
            {
                BindingForResponseDetailInDebuggerPane = new Binding();
                BindingForResponseDetailInDebuggerPane.Mode = BindingMode.OneWay;
                DebuggerResponseGroupsDetail.SetBinding(ItemsControl.ItemsSourceProperty, BindingForResponseDetailInDebuggerPane);
            }
            */

            if (e.AddedItems.Count == 0)
            {
                DebuggerResponseGroupsDetail.SetBinding(ItemsControl.ItemsSourceProperty, (System.Windows.Data.BindingBase)null);
            }
            else
            {
                // icky use of metadata
                ResponseGroup g = e.AddedItems[0].GetType().GetProperty("Val").GetValue(e.AddedItems[0], null) as ResponseGroup;


                Binding b = new Binding();
                b.Source = new ListCollectionView(g.ResponsesList);
                b.Mode = BindingMode.OneWay;
                DebuggerResponseGroupsDetail.SetBinding(ItemsControl.ItemsSourceProperty, b);
            }
        }

        private void DebugTabFactsTextBox_LostFocus(object sender, RoutedEventArgs e)
        {
            DisplayBestRuleForConcept(DebugTabConceptComboBox.Text, DebugTabFactsTextBox.Text);
        }

        private void DebugTabFactsTextBox_KeyDown(object sender, KeyEventArgs e)
        {
            if ( e.Key == Key.Enter )
            {
                DisplayBestRuleForConcept(DebugTabConceptComboBox.Text, DebugTabFactsTextBox.Text);
            }
        }


        void DisplayBestRuleForConcept( string concept, string factstring )
        {
            ObservableCollectionOfRules collection = (ObservableCollectionOfRules)this.FindResource("RulesMatchingDebuggerCriterion");
            collection.Clear();
            System.Collections.Generic.SortedList<string, string> facts = new System.Collections.Generic.SortedList<string, string>();
            facts.Add("concept", concept);
            AddColumnColonToDictionary(facts, factstring);

            foreach ( KeyValuePair<Rule, float> pair in RRSys.FindAllRulesMatchingCriteria(facts) )
            {
                collection.Add(pair);
            }
            /*
            Rule bestMatchingRule = RRSys.FindBestMatchingRule( facts );
            if ( bestMatchingRule != null )
            {
                collection.Add(bestMatchingRule);
            }
             */
        }

        /// <summary>
        /// Temp: turn context1:value1,context2:value2,... into a dict
        /// </summary>
        static void AddColumnColonToDictionary( System.Collections.Generic.SortedList<string, string> facts, 
            string input )
        {
            string[] pairs = input.Split(',');
            foreach( string p in pairs )
            {
                string[] kv = p.Split(':');
                if ( kv.Length >= 2 )   
                    facts.Add(kv[0], kv[1]);
            }
        }  


        /*
        public ObservableCollection<Rule> RulesMatchingDebuggerCriterion
        {
            get { return m_RulesMatchingDebuggerCriterion; }
        }
         */

        /// <summary>
        /// TODO
        /// </summary>
        void QueryResponseSystemWithDebuggerData()
        {
        }


        // ObservableCollection<Rule> m_RulesMatchingDebuggerCriterion;

        #endregion
    }



    public class ObservableCollectionOfRules : ObservableCollection<KeyValuePair<Rule,float>> { };

}
