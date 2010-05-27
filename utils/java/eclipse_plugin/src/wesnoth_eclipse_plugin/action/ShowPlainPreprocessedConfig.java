package wesnoth_eclipse_plugin.action;

import org.eclipse.jface.action.IAction;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.ui.IObjectActionDelegate;
import org.eclipse.ui.IWorkbenchPart;

import wesnoth_eclipse_plugin.globalactions.PreprocessorActions;
import wesnoth_eclipse_plugin.utils.WorkspaceUtils;

public class ShowPlainPreprocessedConfig implements IObjectActionDelegate
{
	public ShowPlainPreprocessedConfig(){}

	@Override
	public void setActivePart(IAction action, IWorkbenchPart targetPart){
	}

	@Override
	public void run(IAction action)
	{
		PreprocessorActions.openPreprocessedFileInEditor(WorkspaceUtils.getSelectedFile(WorkspaceUtils.getWorkbenchWindow()),
				true);
	}

	@Override
	public void selectionChanged(IAction action, ISelection selection){
	}
}
