using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Windows;

using ResponseRulesCLI;
using ManagedAppSystem;

namespace ResponseEditor
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        public App()
        {
            // m_responseDatabase = new ResponseDatabase();
            // Resources["ResponseDatabase"] = m_responseDatabase;
        }

        private void Application_Startup(object sender, StartupEventArgs e)
        {
            // This forces initialization of the app system on start up.
            Resources["AppSystem"] = new AppSystemWrapper();
        }


    }

}
